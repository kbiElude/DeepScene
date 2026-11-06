/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Capture/InjectionLauncher/launcher.h"
#include "app_session.h"
#include <algorithm>
#include <stdio.h>

VRPlayer::AppSession::AppSession()
    :m_depth_strength    (0.0f),
     m_has_depth_strength(false),
     m_has_is_overlay_pushed(false),
     m_is_overlay_pushed (false),
     m_job               (nullptr),
     m_launcher_process  (nullptr),
     m_output_pipe       (nullptr),
     m_state             (SessionState::IDLE),
     m_presenter_process    (nullptr)
{
    /* Stub */
}

VRPlayer::AppSession::~AppSession()
{
    /* What still runs ends with the job. */
    close_handles();
}

void VRPlayer::AppSession::close_handles()
{
    m_channel_ptr.reset();

    HANDLE* const handle_ptr_array[] = {&m_output_pipe, &m_presenter_process, &m_launcher_process, &m_job};

    for (HANDLE* handle_ptr : handle_ptr_array)
    {
        if (*handle_ptr != nullptr)
        {
            ::CloseHandle(*handle_ptr);

            *handle_ptr = nullptr;
        }
    }
}

void VRPlayer::AppSession::end_session(const SessionState& in_state,
                                       const std::string&  in_message)
{
    /* The window, the launcher and the application all belong to the job. */
    if (m_job != nullptr)
    {
        ::TerminateJobObject(m_job,
                             0);
    }

    close_handles();

    m_message = in_message;
    m_state   = in_state;
}

std::string VRPlayer::AppSession::get_command_line(const SessionRequest& in_request)
{
    std::string result = quote_argument(in_request.launcher_path) + " --recipe " + quote_argument(in_request.recipe_path) +
                         " --session " + quote_argument(in_request.session_id);

    if (in_request.is_logging)
    {
        result += " --log";
    }

    if (in_request.arguments.empty() == false)
    {
        result += " --arguments " + quote_argument(in_request.arguments);
    }

    if (in_request.working_directory.empty() == false)
    {
        result += " --working-directory " + quote_argument(in_request.working_directory);
    }

    return result + " --wait " + quote_argument(in_request.executable_path);
}

const std::string& VRPlayer::AppSession::get_message() const
{
    return m_message;
}

VRPlayer::SessionState VRPlayer::AppSession::get_state() const
{
    return m_state;
}

std::string VRPlayer::AppSession::get_presenter_command_line(const SessionRequest& in_request)
{
    return quote_argument(in_request.presenter_path) + " --session " + quote_argument(in_request.session_id) +
           ((in_request.title.empty() ) ? std::string()
                                        : " --title " + quote_argument(in_request.title) ) +
           ((in_request.use_headset)    ? std::string(" --headset")
                                        : std::string() ) +
           ((in_request.use_headset      &&
             in_request.pace_to_headset == false) ? std::string(" --no-pacing")
                                                  : std::string() ) +
           ((in_request.is_logging)     ? std::string(" --log")
                                        : std::string() );
}

void VRPlayer::AppSession::set_is_overlay_pushed(const bool& in_is_pushed)
{
    /* Said when it changes, like the strength. */
    if (m_channel_ptr != nullptr)
    {
        if (m_has_is_overlay_pushed == false ||
            m_is_overlay_pushed     != in_is_pushed)
        {
            m_channel_ptr->set_is_overlay_pushed(in_is_pushed);

            m_is_overlay_pushed     = in_is_pushed;
            m_has_is_overlay_pushed = true;
        }
    }
}

void VRPlayer::AppSession::set_depth_strength(const float& in_depth_strength)
{
    /* Said when it changes, so that what else sets it through the channel is not overridden at every frame. */
    if (m_channel_ptr != nullptr)
    {
        if (m_has_depth_strength == false ||
            m_depth_strength     != in_depth_strength)
        {
            m_channel_ptr->set_depth_strength(in_depth_strength);

            m_depth_strength     = in_depth_strength;
            m_has_depth_strength = true;
        }
    }
}

std::string VRPlayer::AppSession::quote_argument(const std::string& in_argument)
{
    size_t      n_backslashes = 0;
    std::string result        = "\"";

    /* The rules of CommandLineToArgvW: backslashes only count in front of a quote. */
    for (const char character : in_argument)
    {
        if (character == '\\')
        {
            ++n_backslashes;
        }
        else
        if (character == '"')
        {
            result.append(n_backslashes * 2 + 1,
                          '\\');
            result += '"';

            n_backslashes = 0;
        }
        else
        {
            result.append(n_backslashes,
                          '\\');
            result += character;

            n_backslashes = 0;
        }
    }

    result.append(n_backslashes * 2,
                  '\\');

    return result + "\"";
}

std::string VRPlayer::AppSession::read_output(const HANDLE& in_pipe)
{
    char        buffer_i8vec512[512];
    DWORD       n_available = 0;
    DWORD       n_read      = 0;
    std::string result;

    /* The launcher has exited, so everything it wrote is in the pipe, and nothing has to be waited for. */
    while (::PeekNamedPipe(in_pipe,
                           nullptr,
                           0,
                           nullptr,
                          &n_available,
                           nullptr) != FALSE &&
           n_available > 0                    &&
           ::ReadFile(in_pipe,
                      buffer_i8vec512,
                      sizeof(buffer_i8vec512),
                     &n_read,
                      nullptr) != FALSE        &&
           n_read > 0)
    {
        result.append(buffer_i8vec512,
                      n_read);
    }

    return result;
}

bool VRPlayer::AppSession::start(const VRPlayer::AppCatalog::AppEntry& in_app,
                                 const std::string&                    in_launcher_directory,
                                 const uint32_t&                       in_target_eye_width,
                                 const uint32_t&                       in_target_eye_height,
                                 const bool&                           in_use_headset,
                                 const bool&                           in_pace_to_headset,
                                 const bool&                           in_write_log,
                                 std::string*                          out_error_ptr)
{
    Capture::InjectionLauncher::TargetArchitecture   architecture           = Capture::InjectionLauncher::TargetArchitecture::UNKNOWN;
    std::unique_ptr<Stereo::Transport::FrameChannel> channel_ptr;
    HANDLE                                           job                    = nullptr;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION             job_limit              = {};
    std::string                                      launcher_command_line;
    PROCESS_INFORMATION                              launcher_info          = {};
    HANDLE                                           pipe_read              = nullptr;
    HANDLE                                           pipe_write             = nullptr;
    SessionRequest                                   request;
    bool                                             result                 = false;
    SECURITY_ATTRIBUTES                              security               = {};
    STARTUPINFOA                                     startup_info           = {};
    std::string                                      presenter_command_line;
    PROCESS_INFORMATION                              presenter_info         = {};
    HANDLE                                           presenter_process      = nullptr;

    if (m_state == SessionState::RUNNING)
    {
        *out_error_ptr = "An application is running already.";

        goto end;
    }

    if (Capture::InjectionLauncher::Launcher::get_target_architecture(in_app.executable_path,
                                                                     &architecture,
                                                                      out_error_ptr) == false)
    {
        goto end;
    }

    request.arguments         = in_app.arguments;
    request.executable_path   = in_app.executable_path;
    request.is_logging        = in_write_log;
    request.pace_to_headset   = in_pace_to_headset;
    request.launcher_path     = in_launcher_directory + ((architecture == Capture::InjectionLauncher::TargetArchitecture::X86) ? "\\StereoLauncher_x86.exe"
                                                                                                                       : "\\StereoLauncher_x64.exe");
    request.recipe_path       = in_app.recipe_path;
    request.session_id        = std::to_string(::GetCurrentProcessId() ) + "-" + std::to_string(::GetTickCount64() );
    request.title             = in_app.name;
    request.use_headset       = in_use_headset;
    request.presenter_path       = in_launcher_directory + "\\StereoPresenter_x64.exe";
    request.working_directory = in_app.working_directory;

    if (::GetFileAttributesA(request.launcher_path.c_str() ) == INVALID_FILE_ATTRIBUTES)
    {
        *out_error_ptr = "The launcher for " + std::string((architecture == Capture::InjectionLauncher::TargetArchitecture::X86) ? "32-bit"
                                                                                                                          : "64-bit") +
                         " applications is not installed: " + request.launcher_path;

        goto end;
    }

    if (::GetFileAttributesA(request.presenter_path.c_str() ) == INVALID_FILE_ATTRIBUTES)
    {
        *out_error_ptr = "The window which shows the application is not installed: " + request.presenter_path;

        goto end;
    }

    /* The channel is made before anything is started, so that what is said through it is there when the converter first looks. */
    channel_ptr = Stereo::Transport::FrameChannel::open_ptr(request.session_id,
                                                            out_error_ptr);

    if (channel_ptr == nullptr)
    {
        goto end;
    }

    /* What the application renders is made as large as fits an eye of the headset; the channel has room for no more than it has. */
    if (in_target_eye_width  > 0 &&
        in_target_eye_height > 0)
    {
        channel_ptr->set_target_eye_size(std::min<uint32_t>(in_target_eye_width,
                                                            Stereo::Transport::FrameChannel::get_max_width() / 2),
                                         std::min<uint32_t>(in_target_eye_height,
                                                            Stereo::Transport::FrameChannel::get_max_height() ));
    }

    /* Everything which is started belongs to a job, which ends it all when the session ends. */
    job = ::CreateJobObjectW(nullptr,
                             nullptr);

    job_limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    {
        BOOL is_limit_set = FALSE;

        if (job != nullptr)
        {
            is_limit_set = ::SetInformationJobObject(job,
                                                     JobObjectExtendedLimitInformation,
                                                    &job_limit,
                                                     sizeof(job_limit) );
        }

        if (is_limit_set == FALSE)
        {
            *out_error_ptr = "The session cannot be kept together.";

            goto end;
        }
    }

    /* What the launcher says is kept, so that the window can show why it stopped. */
    security.bInheritHandle = TRUE;
    security.nLength        = sizeof(security);

    if (::CreatePipe(&pipe_read,
                     &pipe_write,
                     &security,
                     0) == FALSE)
    {
        *out_error_ptr = "The output of the launcher cannot be kept.";

        goto end;
    }

    ::SetHandleInformation(pipe_read,
                           HANDLE_FLAG_INHERIT,
                           0);

    launcher_command_line = get_command_line(request);
    startup_info.cb       = sizeof(startup_info);
    presenter_command_line   = get_presenter_command_line(request);

    /* The window first: it is there when the first image comes. Both are suspended until they are in the job, so that nothing they
     * start can get out of it.
     */
    if (::CreateProcessA(nullptr,
                         &presenter_command_line[0],
                         nullptr,                      /* lpProcessAttributes */
                         nullptr,                      /* lpThreadAttributes  */
                         FALSE,                        /* bInheritHandles     */
                         CREATE_SUSPENDED,
                         nullptr,                      /* lpEnvironment       */
                         nullptr,                      /* lpCurrentDirectory  */
                        &startup_info,
                        &presenter_info) == FALSE)
    {
        *out_error_ptr = "The window which shows the application cannot be started: " + request.presenter_path;

        goto end;
    }

    presenter_process = presenter_info.hProcess;

    ::AssignProcessToJobObject(job,
                               presenter_process);
    ::ResumeThread            (presenter_info.hThread);
    ::CloseHandle             (presenter_info.hThread);

    startup_info.dwFlags    = STARTF_USESTDHANDLES;
    startup_info.hStdError  = pipe_write;
    startup_info.hStdOutput = pipe_write;

    if (::CreateProcessA(nullptr,
                         &launcher_command_line[0],
                         nullptr,                      /* lpProcessAttributes */
                         nullptr,                      /* lpThreadAttributes  */
                         TRUE,                         /* bInheritHandles     */
                         CREATE_NO_WINDOW | CREATE_SUSPENDED,
                         nullptr,                      /* lpEnvironment       */
                         nullptr,                      /* lpCurrentDirectory  */
                        &startup_info,
                        &launcher_info) == FALSE)
    {
        *out_error_ptr = "The launcher cannot be started: " + request.launcher_path;

        goto end;
    }

    ::AssignProcessToJobObject(job,
                               launcher_info.hProcess);
    ::ResumeThread            (launcher_info.hThread);
    ::CloseHandle             (launcher_info.hThread);

    /* A session which ended before is replaced; its job is closed now. */
    close_handles();

    m_message.clear();

    m_channel_ptr        = std::move(channel_ptr);
    m_has_depth_strength = false;

    m_has_is_overlay_pushed = false;
    m_job                = job;
    m_launcher_process   = launcher_info.hProcess;
    m_output_pipe        = pipe_read;
    m_state              = SessionState::RUNNING;
    m_presenter_process     = presenter_process;
    job                  = nullptr;
    pipe_read            = nullptr;
    result               = true;
    presenter_process       = nullptr;

end:
    /* A job which is not kept takes what was started in it along. */
    if (job != nullptr)
    {
        ::CloseHandle(job);
    }

    if (presenter_process != nullptr)
    {
        ::CloseHandle(presenter_process);
    }

    if (pipe_read != nullptr)
    {
        ::CloseHandle(pipe_read);
    }

    if (pipe_write != nullptr)
    {
        ::CloseHandle(pipe_write);
    }

    return result;
}

void VRPlayer::AppSession::update()
{
    DWORD exit_code = 0;

    if (m_state == SessionState::RUNNING)
    {
        if (::WaitForSingleObject(m_launcher_process,
                                  0) == WAIT_OBJECT_0)
        {
            ::GetExitCodeProcess(m_launcher_process,
                                &exit_code);

            if (exit_code == 0)
            {
                end_session(SessionState::ENDED,
                            std::string() );
            }
            else
            {
                const std::string output  = read_output(m_output_pipe);
                const std::string message = (output.empty() ) ? "The launcher stopped with the code " + std::to_string(exit_code) + "."
                                                              : output;

                end_session(SessionState::FAILED,
                            message);
            }
        }
        else
        if (::WaitForSingleObject(m_presenter_process,
                                  0) == WAIT_OBJECT_0)
        {
            /* The window was closed: that is the end of the session, and of the application with it. */
            end_session(SessionState::ENDED,
                        std::string() );
        }
    }
}

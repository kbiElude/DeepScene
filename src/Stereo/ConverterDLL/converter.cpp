/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/RecipeLoader/recipe_loader.h"
#include "Capture/APIDumpLoader/api_info.h"
#include "Common/frame_boundary.h"
#include "Interception/Hooks/D3D8/types.h"
#include "Stereo/Session/session_environment.h"
#include "converter.h"
#include "own_call_scope.h"
#include <array>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>

Stereo::ConverterDLL::Converter::Converter(const Stereo::Session::SessionInfo&              in_session_info,
                                           const Analysis::RecipeLoader::Recipe&            in_recipe,
                                           std::unique_ptr<Stereo::Transport::FrameChannel> in_channel_ptr)
    :m_channel_ptr(std::move(in_channel_ptr) ),
     m_log        (in_session_info)
{
    /* A converter that finds that it cannot convert the application right ends it, and says why in the log. */
    m_live_state.core_ptr.reset(new Stereo::Core::StereoCore(in_recipe,
                                                             [this](const std::string& in_message)
                                                             {
                                                                 m_log.write_failure("The application cannot be converted: " + in_message);

                                                                 ::TerminateProcess(::GetCurrentProcess(),
                                                                                    Stereo::Session::SessionEnvironment::get_converter_failed_exit_code() );
                                                             }) );

    /* The calls that the front end of Direct3D 8 follows, before and after. A frame is passed on before the present, since what the
     * back buffer holds after a present is not defined.
     */
    for (uint32_t n_api_func = 0;
                  n_api_func < APIInterceptor::APIFUNCTION_COUNT;
                ++n_api_func)
    {
        const auto function = static_cast<APIInterceptor::APIFunction>(n_api_func);

        /* The rest of the converter (the eye renderer, the scaler, the exporter) is for Direct3D 8: of the calls of Direct3D 7 it only
         * follows what the application does (see on_d3d7_pre()), until the ones of Direct3D 7 are made.
         */
        if (CallFeeder::is_followed(function) )
        {
            APIInterceptor::register_for_post_callback(function,
                                                      &Converter::on_post_callback,
                                                       this);
            APIInterceptor::register_for_pre_callback (function,
                                                      &Converter::on_pre_callback,
                                                       this);
        }
    }

    /* Ways to set what the program which shows the images says otherwise, for a run without it, and to look at a frame. */
    {
        float value = 0.0f;

        if (read_environment_number("APISTEREO_EYE_SEPARATION",
                                   &value) )
        {
            m_live_state.core_ptr->set_eye_separation(value);

            m_log.write("The eye separation is " + std::to_string(value) + ", from APISTEREO_EYE_SEPARATION");
        }

        if (read_environment_number("APISTEREO_DEPTH_STRENGTH",
                                   &value) )
        {
            m_live_state.core_ptr->set_depth_strength(value);

            m_log.write("The strength of the 3D is " + std::to_string(value) + ", from APISTEREO_DEPTH_STRENGTH");
        }

        if (read_environment_number("APISTEREO_PUSH_OVERLAY",
                                   &value) )
        {
            m_live_state.core_ptr->set_is_overlay_pushed(value != 0.0f);

            m_live_state.is_overlay_push_forced = true;

            m_log.write("What the recipe leaves alone is pushed toward the viewer: " + std::to_string(value != 0.0f) + ", from APISTEREO_PUSH_OVERLAY");
        }

        if (read_environment_number("APISTEREO_DUMP_EVERY",
                                   &value) )
        {
            m_live_state.dump_every = static_cast<uint64_t>(value);
        }

        if (read_environment_number("APISTEREO_DUMP_FRAME",
                                   &value) )
        {
            m_live_state.dump_frame = static_cast<uint64_t>(value);
        }
    }

    m_live_state.d3d7_renderer_ptr.reset(new D3D7EyeRenderer(m_live_state.core_ptr.get(),
                                                           &m_live_state.d3d7_windowed_mode) );
    m_live_state.renderer_ptr.reset(new D3D8EyeRenderer(m_live_state.core_ptr.get                               () ));
    m_frame_passer_ptr.reset       (new FramePasser    (std::unique_ptr<Stereo::Transport::IFrameExporter>(new D3D8HostMemoryExporter(m_channel_ptr.get() )),
                                                        m_log) );
    m_d3d7_frame_passer_ptr.reset  (new FramePasser    (std::unique_ptr<Stereo::Transport::IFrameExporter>(new D3D7HostMemoryExporter(m_channel_ptr.get() )),
                                                        m_log) );

    {
        /* Said by the program which shows the images, before it starts the application: the device is made with it. */
        uint32_t target_height = 0;
        uint32_t target_width  = 0;

        bool is_shown_elsewhere = m_channel_ptr->get_target_eye_size(&target_width,
                                                                     &target_height);

        /* A way to say it for a run without VRPlayer, which says it otherwise. */
        {
            float height = 0.0f;
            float width  = 0.0f;

            if (is_shown_elsewhere == false                          &&
                read_environment_number("APISTEREO_TARGET_EYE_WIDTH",
                                       &width)                        &&
                read_environment_number("APISTEREO_TARGET_EYE_HEIGHT",
                                       &height) )
            {
                target_height      = static_cast<uint32_t>(height);
                target_width       = static_cast<uint32_t>(width);
                is_shown_elsewhere = true;

                m_log.write("The images of the eyes are " + std::to_string(target_width) + " x " + std::to_string(target_height) + ", from APISTEREO_TARGET_EYE_WIDTH and APISTEREO_TARGET_EYE_HEIGHT");
            }
        }

        /* A program that shows the images says how large they are before the application starts: without one the application is on the screen, as
         * it is, and a window of it is not made invisible.
         */
        m_live_state.d3d7_windowed_mode.set_target_eye_size(target_width,
                                                           target_height);
        m_live_state.d3d7_windowed_mode.set_is_enabled(is_shown_elsewhere);

        m_live_state.scaler_ptr.reset(new D3D8ResolutionScaler(target_width,
                                                               target_height,
                                                               m_live_state.core_ptr.get(),
                                                               [this]()
                                                               {
                                                                   return m_live_state.renderer_ptr->get_render_target_ptr();
                                                               },
                                                               [this](const std::string& in_message)
                                                               {
                                                                   m_log.write(in_message);
                                                               }) );

        /* The analysis sees the application as it asked to be, or the signatures of its draws would depend on the scale. */
        m_live_state.feeder.set_chunk_func([this](const APIInterceptor::APIFunction& in_function,
                                                  std::vector<uint8_t>*              inout_chunk_ptr)
                                           {
                                               m_live_state.scaler_ptr->restore_chunk(in_function,
                                                                                      inout_chunk_ptr);
                                           });
    }

    m_log.write("The converter is loaded");
}

Stereo::ConverterDLL::Converter::~Converter()
{
    m_log.write("The converter is removed");
}

void Stereo::ConverterDLL::Converter::count_draw(const Analysis::RecipeLoader::DrawClass& in_draw_class)
{
    switch (in_draw_class)
    {
        case Analysis::RecipeLoader::DrawClass::SKIPPED:
        {
            m_live_state.n_skipped_draws++;

            break;
        }

        case Analysis::RecipeLoader::DrawClass::UNAFFECTED:
        {
            m_live_state.n_unaffected_draws++;

            break;
        }

        case Analysis::RecipeLoader::DrawClass::AFFECTED:
        default:
        {
            m_live_state.n_affected_draws++;

            break;
        }
    }
}

std::unique_ptr<Stereo::ConverterDLL::Converter> Stereo::ConverterDLL::Converter::create_ptr()
{
    std::string                  error;
    std::unique_ptr<Converter>   result_ptr;
    Stereo::Session::SessionInfo session_info;

    if (Stereo::Session::SessionEnvironment::read(&session_info) )
    {
        Analysis::RecipeLoader::Recipe recipe;
        auto                           channel_ptr = Stereo::Transport::FrameChannel::open_ptr(session_info.session_id,
                                                                                              &error);

        if (channel_ptr != nullptr)
        {
            const bool is_recipe_loaded = Analysis::RecipeLoader::RecipeLoader::load(session_info.recipe_path,
                                                                                    &recipe,
                                                                                    &error);

            if (is_recipe_loaded)
            {
                result_ptr.reset(new Converter(session_info,
                                               recipe,
                                               std::move(channel_ptr) ));
            }
        }
    }

    return result_ptr;
}

std::string Stereo::ConverterDLL::Converter::make_statistics_line() const
{
    const uint64_t n_failed_twins = m_live_state.d3d7_renderer_ptr->get_n_failed_twins();
    const uint64_t n_twins        = m_live_state.d3d7_renderer_ptr->get_n_twins       ();
    LARGE_INTEGER frequency;

    ::QueryPerformanceFrequency(&frequency);

    const double n_frames_d       = static_cast<double>((m_live_state.n_frames > 0) ? m_live_state.n_frames : 1);
    const std::string cost_text   = ", the callbacks took " + std::to_string(1000.0 * static_cast<double>(m_live_state.n_callback_ticks.load() ) / static_cast<double>(frequency.QuadPart) / n_frames_d) + " ms a frame, " + std::to_string(1000.0 * static_cast<double>(m_live_state.n_draw_ticks.load() ) / static_cast<double>(frequency.QuadPart) / n_frames_d) + " of them in the draws of Direct3D 7";
    const std::string twin_text   = (n_twins + n_failed_twins > 0) ? ", "                                  +
                                                                       std::to_string(n_twins)         +
                                                                       " surfaces with a twin, "       +
                                                                       std::to_string(n_failed_twins)  +
                                                                       " twins that could not be made, "   +
                                                                       std::to_string(m_live_state.d3d7_renderer_ptr->get_n_unmirrored_calls() ) +
                                                                       " calls not made for the right eye" +
                                                                       ", "                                +
                                                                       std::to_string(m_live_state.d3d7_renderer_ptr->get_n_pixel_calls() ) +
                                                                       " calls on the back buffer in pixels it no longer has"
                                                                     : std::string();

    return "After "                                                       +
           std::to_string(m_live_state.n_frames)                          +
           " frames: "                                                    +
           std::to_string(m_live_state.n_affected_draws)                  +
           " affected draws, "                                            +
           std::to_string(m_live_state.n_unaffected_draws)                +
           " unaffected, "                                                +
           std::to_string(m_live_state.n_skipped_draws)                   +
            " skipped, "                                                  +
           std::to_string(m_live_state.core_ptr->get_n_unknown_draws() )  +
           " of a signature the recipe does not know, "                   +
           std::to_string(m_live_state.core_ptr->get_n_unknown_frames() ) +
           " frames of a scene it has no distance for, "                  +
           std::to_string(m_live_state.n_kept_matrices)                   +
           " matrices kept"                                               +
           twin_text                                                      +
           cost_text;
}

void Stereo::ConverterDLL::Converter::on_d3d7_frame_ending(const APIInterceptor::APIFunction&         in_api_func,
                                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                           std::vector<std::string>*                  out_log_line_vec_ptr)
{
    constexpr uint64_t N_FRAMES_BETWEEN_STATISTICS = 600;

    /* The frame which a flip is about to show is in the back buffer of the surface it is called for. */
    if (in_api_func == APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP)
    {
        pass_d3d7_frame(const_cast<void*>(in_arguments_ptr[0].get_ptr() ) );

        m_live_state.d3d7_windowed_mode.keep_windows_invisible();
    }

    m_live_state.n_frames++;

    if (m_live_state.n_frames % N_FRAMES_BETWEEN_STATISTICS == 0)
    {
        out_log_line_vec_ptr->push_back(make_statistics_line() );
    }

    read_channel_settings(out_log_line_vec_ptr);
}

void Stereo::ConverterDLL::Converter::on_post_callback(APIInterceptor::APIFunction                in_api_func,
                                                       void*                                      in_user_arg_ptr,
                                                       uint32_t                                   in_n_arguments_out,
                                                       const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr,
                                                       const APIInterceptor::APIFunctionArgument* in_returned_value_ptr)
{
    auto* this_ptr = static_cast<Converter*>(in_user_arg_ptr);

    if (OwnCallScope::is_active() == false)
    {
        std::lock_guard<std::mutex> lock(this_ptr->m_live_state.mutex);

        uint64_t shape_key = 0;

        this_ptr->m_live_state.feeder.on_post(in_api_func,
                                              in_n_arguments_out,
                                              in_arguments_out_ptr,
                                              in_returned_value_ptr);

        /* What the frame that ended was made of tells which scene it is; the draws that follow are converged for it. */
        if (this_ptr->m_live_state.feeder.take_completed_frame_shape_key(&shape_key) )
        {
            this_ptr->m_live_state.core_ptr->on_frame_completed(shape_key);
        }

        /* The eye renderer is for Direct3D 8; the calls of Direct3D 7 are only followed. */
        if (Capture::APIDumpLoader::get_api_group(in_api_func) == Capture::APIDumpLoader::APIGroup::D3D8)
        {
            this_ptr->m_live_state.renderer_ptr->on_post(in_api_func,
                                                         in_n_arguments_out,
                                                         in_arguments_out_ptr);
        }
        else
        {
            this_ptr->m_live_state.d3d7_renderer_ptr->on_post(in_api_func,
                                                              in_n_arguments_out,
                                                              in_arguments_out_ptr,
                                                              in_returned_value_ptr);
        }
    }

    /* The frame is presented: the application makes the next one when the screen wants it, which is what the program that shows the
     * frames says (if it does). Outside the lock of the converter, since it is as long as a frame.
     */
    if (OwnCallScope::is_active() == false &&
        in_api_func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT)
    {
        this_ptr->m_frame_passer_ptr->wait_for_pace();
    }
    else
    if (OwnCallScope::is_active() == false &&
        in_api_func == APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP)
    {
        this_ptr->m_d3d7_frame_passer_ptr->wait_for_pace();
    }
}

void Stereo::ConverterDLL::Converter::on_pre(const APIInterceptor::APIFunction&         in_api_func,
                                             const uint32_t&                            in_n_arguments,
                                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                             bool*                                      out_should_pass_through_ptr)
{
    const bool               is_present        = (in_api_func    == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT &&
                                                  in_n_arguments >  0);
    std::vector<std::string> log_line_vec;
    void*                    right_surface_ptr = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_live_state.mutex);

        const bool is_d3d8 = (Capture::APIDumpLoader::get_api_group(in_api_func) == Capture::APIDumpLoader::APIGroup::D3D8);
        const bool is_draw = m_live_state.feeder.on_pre(in_api_func,
                                                        in_n_arguments,
                                                        in_arguments_ptr);

        if (in_api_func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM)
        {
            /* The matrix is kept; it is put in the device before the draws that need it, for the eye they are made for. */
            *out_should_pass_through_ptr = (on_set_transform(in_n_arguments,
                                                             in_arguments_ptr) == false);
        }

        if (is_d3d8)
        {
            m_live_state.scaler_ptr->on_pre(in_api_func,
                                            in_n_arguments,
                                            in_arguments_ptr);
        }

        if (is_draw == false)
        {
            if (is_d3d8)
            {
                m_live_state.renderer_ptr->on_pre(in_api_func,
                                                  in_n_arguments,
                                                  in_arguments_ptr);
            }
            else
            {
                /* What a window cannot have is made in its way, and not by the call of the application. */
                if (m_live_state.d3d7_windowed_mode.on_pre(in_api_func,
                                                           in_n_arguments,
                                                           in_arguments_ptr) == false)
                {
                    *out_should_pass_through_ptr = false;
                }

                /* What is in pixels is made in the pixels of a larger back buffer, if it is made larger. */
                if (m_live_state.d3d7_renderer_ptr->on_pre(in_api_func,
                                                           in_n_arguments,
                                                           in_arguments_ptr) == false)
                {
                    *out_should_pass_through_ptr = false;
                }

                if (APIInterceptor::is_frame_boundary(in_api_func,
                                                      in_n_arguments,
                                                      in_arguments_ptr) )
                {
                    on_d3d7_frame_ending(in_api_func,
                                         in_arguments_ptr,
                                        &log_line_vec);
                }
            }
        }
        else
        {
            const auto draw_class = m_live_state.core_ptr->classify_draw(m_live_state.feeder.get_last_draw_signature_key(),
                                                                                      m_live_state.feeder.get_last_draw_cover_key() );

            /* A draw of Direct3D 7 is made by the renderer if it moves the vertices for the eye. */
            if (is_d3d8)
            {
                *out_should_pass_through_ptr = m_live_state.renderer_ptr->on_pre_draw(draw_class,
                                                                                      in_arguments_ptr);
            }
            else
            {
                LARGE_INTEGER draw_end_qpc;
                LARGE_INTEGER draw_start_qpc;

                ::QueryPerformanceCounter(&draw_start_qpc);

                *out_should_pass_through_ptr = m_live_state.d3d7_renderer_ptr->on_pre_draw(in_api_func,
                                                                                           in_n_arguments,
                                                                                           in_arguments_ptr,
                                                                                           m_live_state.feeder.get_viewport().width);

                ::QueryPerformanceCounter(&draw_end_qpc);

                m_live_state.n_draw_ticks += draw_end_qpc.QuadPart - draw_start_qpc.QuadPart;
            }

            count_draw(draw_class);
        }

        if (is_present)
        {
            on_present_starting(in_arguments_ptr,
                               &right_surface_ptr,
                               &log_line_vec);
        }
    }

    for (const auto& log_line : log_line_vec)
    {
        m_log.write(log_line);
    }

    if (is_present)
    {
        m_frame_passer_ptr->on_present(const_cast<void*>(in_arguments_ptr[0].get_ptr() ),
                                       nullptr, /* in_left_surface_ptr */
                                       right_surface_ptr);
    }
}

void Stereo::ConverterDLL::Converter::on_pre_callback(APIInterceptor::APIFunction                in_api_func,
                                                      uint32_t                                   in_n_arguments,
                                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                      void*                                      in_user_arg_ptr,
                                                      bool*                                      out_should_pass_through_ptr)
{
    if (OwnCallScope::is_active() == false)
    {
        LARGE_INTEGER end_qpc;
        LARGE_INTEGER start_qpc;

        ::QueryPerformanceCounter(&start_qpc);

        static_cast<Converter*>(in_user_arg_ptr)->on_pre(in_api_func,
                                                         in_n_arguments,
                                                         in_arguments_ptr,
                                                         out_should_pass_through_ptr);

        ::QueryPerformanceCounter(&end_qpc);

        static_cast<Converter*>(in_user_arg_ptr)->m_live_state.n_callback_ticks += end_qpc.QuadPart - start_qpc.QuadPart;
    }
}

void Stereo::ConverterDLL::Converter::on_present_starting(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                          void**                                     out_right_surface_ptr_ptr,
                                                          std::vector<std::string>*                  out_log_line_vec_ptr)
{
    constexpr uint64_t N_FRAMES_BETWEEN_STATISTICS = 600;

    *out_right_surface_ptr_ptr = m_live_state.renderer_ptr->get_back_buffer_twin_ptr(const_cast<void*>(in_arguments_ptr[0].get_ptr() ));

    /* The application renders to a window that cannot be seen; it may change its style. */
    m_windowed_mode.keep_windows_invisible();

    m_live_state.n_frames++;

    if (m_live_state.n_frames % N_FRAMES_BETWEEN_STATISTICS == 0)
    {
        out_log_line_vec_ptr->push_back(make_statistics_line() );
    }

    read_channel_settings(out_log_line_vec_ptr);
}

bool Stereo::ConverterDLL::Converter::on_set_transform(const uint32_t&                            in_n_arguments,
                                                       const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t MATRIX_ARGUMENT_INDEX = 2;
    constexpr uint32_t STATE_ARGUMENT_INDEX  = 1;

    const void*           matrix_data_ptr = nullptr;
    std::array<float, 16> matrix;
    uint32_t              n_matrix_bytes  = 0;
    bool                  result          = false;

    if (in_n_arguments                               >  MATRIX_ARGUMENT_INDEX                 &&
        in_arguments_ptr[MATRIX_ARGUMENT_INDEX].type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        const bool has_chunk = APIInterceptor::get_data_chunk(in_arguments_ptr[MATRIX_ARGUMENT_INDEX].get_data_chunk_id(),
                                                             &matrix_data_ptr,
                                                             &n_matrix_bytes);

        if (has_chunk)
        {
            if (n_matrix_bytes == sizeof(matrix) )
            {
                ::memcpy(matrix.data(),
                         matrix_data_ptr,
                         sizeof(matrix) );

                if (in_arguments_ptr[STATE_ARGUMENT_INDEX].get_u32() == Interception::Hooks::D3D8::D3DTS_VIEW)
                {
                    m_live_state.core_ptr->on_view_matrix_set(matrix);
                }

                result = m_live_state.core_ptr->on_matrix_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM,
                                                               std::vector<APIInterceptor::APIFunctionArgument>(in_arguments_ptr,
                                                                                                                in_arguments_ptr + in_n_arguments),
                                                               matrix);

                if (result)
                {
                    m_live_state.n_kept_matrices++;
                }
            }
        }
    }

    return result;
}

void Stereo::ConverterDLL::Converter::pass_d3d7_frame(void* in_primary_surface_ptr)
{
    constexpr uint32_t BACK_BUFFER_CAPS = 0x00000004; /* DDSCAPS_BACKBUFFER */

    const OwnCallScope                              own_call_scope;
    Interception::Hooks::D3D7::IDirectDrawSurface7* back_buffer_ptr = static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(m_live_state.d3d7_windowed_mode.get_back_buffer_ptr() );
    bool                                            is_to_release   = false;

    /* The application renders to the back buffer of its window if it is made to; otherwise the chain it flips with has one. */
    if (back_buffer_ptr == nullptr)
    {
        Interception::Hooks::D3D7::DDSCAPS2 caps   = {};
        void** const                        vtable = *reinterpret_cast<void***>(in_primary_surface_ptr);

        caps.dwCaps = BACK_BUFFER_CAPS;

        is_to_release = SUCCEEDED(reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE>(vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE])(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(in_primary_surface_ptr),
                                                                                                                                                                                                         &caps,
                                                                                                                                                                                                         &back_buffer_ptr) );

        if (is_to_release == false)
        {
            back_buffer_ptr = nullptr;
        }
    }

    if (back_buffer_ptr != nullptr)
    {
        /* A debugging aid: the images of the eyes of one frame, to look at. */
        if ( (m_live_state.dump_frame != 0 &&
              m_live_state.n_frames   == m_live_state.dump_frame)    ||
             (m_live_state.dump_every != 0 &&
              m_live_state.n_frames   % m_live_state.dump_every == 0) )
        {
            char temp_i8vec260[MAX_PATH] = {};

            if (::GetTempPathA(MAX_PATH,
                               temp_i8vec260) != 0)
            {
                const bool is_written = D3D7HostMemoryExporter::write_debug_image(back_buffer_ptr,
                                                                                  m_live_state.d3d7_renderer_ptr->get_twin_ptr(back_buffer_ptr),
                                                                                  std::string(temp_i8vec260) + "apistereo_frame_" + std::to_string(m_live_state.n_frames) + ".bmp");

                m_log.write((is_written) ? "A frame is written to a file" : "A frame cannot be written to a file");
            }
        }

        m_d3d7_frame_passer_ptr->on_present(nullptr, /* in_device_ptr */
                                            back_buffer_ptr,
                                            m_live_state.d3d7_renderer_ptr->get_twin_ptr(back_buffer_ptr) );

        if (is_to_release)
        {
            void** const back_buffer_vtable = *reinterpret_cast<void***>(back_buffer_ptr);

            reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_RELEASE>(back_buffer_vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_RELEASE])(back_buffer_ptr);
        }
    }
}

void Stereo::ConverterDLL::Converter::read_channel_settings(std::vector<std::string>* out_log_line_vec_ptr)
{
    float convergence_distance = 0.0f;
    float depth_strength       = 0.0f;
    float eye_separation       = 0.0f;

    const bool has_eye_separation = m_channel_ptr->get_eye_separation(&eye_separation);

    if (has_eye_separation)
    {
        if (eye_separation != m_live_state.core_ptr->get_eye_separation() )
        {
            m_live_state.core_ptr->set_eye_separation(eye_separation);

            out_log_line_vec_ptr->push_back("The eye separation is now " + std::to_string(eye_separation) );
        }
    }

    const bool has_depth_strength = m_channel_ptr->get_depth_strength(&depth_strength);

    if (has_depth_strength)
    {
        if (depth_strength != m_live_state.core_ptr->get_depth_strength() )
        {
            m_live_state.core_ptr->set_depth_strength(depth_strength);

            out_log_line_vec_ptr->push_back("The strength of the 3D is now " + std::to_string(depth_strength) );
        }
    }

    float ipd_scale = 1.0f;

    const bool has_ipd_scale = m_channel_ptr->get_ipd_scale(&ipd_scale);

    if (has_ipd_scale)
    {
        if (ipd_scale != m_live_state.core_ptr->get_ipd_scale() )
        {
            m_live_state.core_ptr->set_ipd_scale(ipd_scale);

            out_log_line_vec_ptr->push_back("The scale of the eyes of the person is now " + std::to_string(ipd_scale) );
        }
    }

    const bool is_overlay_pushed = m_channel_ptr->get_is_overlay_pushed();

    if (m_live_state.is_overlay_push_forced == false &&
        is_overlay_pushed != m_live_state.core_ptr->get_is_overlay_pushed() )
    {
        m_live_state.core_ptr->set_is_overlay_pushed(is_overlay_pushed);

        out_log_line_vec_ptr->push_back((is_overlay_pushed) ? "What the recipe leaves alone is now pushed toward the viewer"
                                                            : "What the recipe leaves alone is no longer pushed toward the viewer");
    }

    const bool has_convergence_distance = m_channel_ptr->get_convergence_distance(&convergence_distance);

    if (has_convergence_distance)
    {
        if (convergence_distance != m_live_state.core_ptr->get_convergence_distance() )
        {
            m_live_state.core_ptr->set_convergence_distance(convergence_distance);

            out_log_line_vec_ptr->push_back("The convergence distance is now " + std::to_string(convergence_distance) );
        }
    }
}

bool Stereo::ConverterDLL::Converter::read_environment_number(const char* in_name_ptr,
                                                               float*      out_value_ptr)
{
    char                value_i8vec32[32] = {};
    const unsigned long n_chars           = ::GetEnvironmentVariableA(in_name_ptr,
                                                                      value_i8vec32,
                                                                      static_cast<DWORD>(sizeof(value_i8vec32) ));
    const bool          result            = (n_chars > 0                      &&
                                             n_chars < sizeof(value_i8vec32) );

    if (result)
    {
        *out_value_ptr = static_cast<float>(::atof(value_i8vec32) );
    }

    return result;
}

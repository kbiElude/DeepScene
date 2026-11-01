/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "win32_call_executor.h"
#include <algorithm>
#include <string.h>

/* The window class of the windows whose class the dump does not name (an atom). */
static const wchar_t DEFAULT_WINDOW_CLASS_NAME[] = L"StereoReplayEngineDLLWindow";

/* The property of a window which holds the executor it belongs to. */
static const wchar_t EXECUTOR_PROPERTY_NAME[]    = L"StereoReplayEngineDLLExecutor";

typedef HGLRC (WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
typedef BOOL  (WINAPI* PFNWGLSWAPINTERVALEXTPROC)        (int);

Replay::StereoReplayEngineDLL::Win32CallExecutor::Win32CallExecutor(ObjectRemapper*         in_remapper_ptr,
                                                                    const WindowClosedFunc& in_window_closed_func,
                                                                    const WindowPlacedFunc& in_window_placed_func)
    :m_are_windows_visible(true),
     m_has_last_placement(false),
     m_remapper_ptr      (in_remapper_ptr),
     m_window_closed_func(in_window_closed_func),
     m_window_placed_func(in_window_placed_func)
{
    const struct
    {
        APIInterceptor::APIFunction function;
        HandlerFuncPtr              handler_func_ptr;
    } handler_array[] =
    {
        {APIInterceptor::APIFUNCTION_GDI32_CHOOSEPIXELFORMAT,                  &Win32CallExecutor::on_ignored_call},
        {APIInterceptor::APIFUNCTION_GDI32_DESCRIBEPIXELFORMAT,                &Win32CallExecutor::on_ignored_call},
        {APIInterceptor::APIFUNCTION_GDI32_GETPIXELFORMAT,                     &Win32CallExecutor::on_ignored_call},
        {APIInterceptor::APIFUNCTION_GDI32_SETPIXELFORMAT,                     &Win32CallExecutor::on_set_pixel_format},
        {APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS,                        &Win32CallExecutor::on_swap_buffers},
        {APIInterceptor::APIFUNCTION_USER32_ADJUSTWINDOWRECTEX,                &Win32CallExecutor::on_ignored_call},
        {APIInterceptor::APIFUNCTION_USER32_CREATEDCA,                         &Win32CallExecutor::on_ignored_call},
        {APIInterceptor::APIFUNCTION_USER32_CREATEDCW,                         &Win32CallExecutor::on_ignored_call},
        {APIInterceptor::APIFUNCTION_USER32_CREATEWINDOWEXA,                   &Win32CallExecutor::on_create_window_ex_a},
        {APIInterceptor::APIFUNCTION_USER32_CREATEWINDOWEXW,                   &Win32CallExecutor::on_create_window_ex_w},
        {APIInterceptor::APIFUNCTION_USER32_GETDC,                             &Win32CallExecutor::on_get_dc},
        {APIInterceptor::APIFUNCTION_USER32_RELEASEDC,                         &Win32CallExecutor::on_release_dc},
        {APIInterceptor::APIFUNCTION_WGL_WGLCOPYCONTEXT,                       &Win32CallExecutor::on_copy_context},
        {APIInterceptor::APIFUNCTION_WGL_WGLCREATECONTEXT,                     &Win32CallExecutor::on_create_context},
        {APIInterceptor::APIFUNCTION_WGL_WGLCREATECONTEXTATTRIBSARB,           &Win32CallExecutor::on_create_context_attribs_arb},
        {APIInterceptor::APIFUNCTION_WGL_WGLCREATELAYERCONTEXT,                &Win32CallExecutor::on_create_layer_context},
        {APIInterceptor::APIFUNCTION_WGL_WGLDELETECONTEXT,                     &Win32CallExecutor::on_delete_context},
        {APIInterceptor::APIFUNCTION_WGL_WGLGETCURRENTCONTEXT,                 &Win32CallExecutor::on_ignored_call},
        {APIInterceptor::APIFUNCTION_WGL_WGLGETCURRENTDC,                      &Win32CallExecutor::on_ignored_call},
        {APIInterceptor::APIFUNCTION_WGL_WGLGETEXTENSIONSSTRINGARB,            &Win32CallExecutor::on_ignored_call},
        {APIInterceptor::APIFUNCTION_WGL_WGLGETSWAPINTERVALEXT,                &Win32CallExecutor::on_ignored_call},
        {APIInterceptor::APIFUNCTION_WGL_WGLMAKECURRENT,                       &Win32CallExecutor::on_make_current},
        {APIInterceptor::APIFUNCTION_WGL_WGLSHARELISTS,                        &Win32CallExecutor::on_share_lists},
        {APIInterceptor::APIFUNCTION_WGL_WGLSWAPINTERVALEXT,                   &Win32CallExecutor::on_swap_interval_ext},
        {APIInterceptor::APIFUNCTION_WGL_WGLSWAPLAYERBUFFERS,                  &Win32CallExecutor::on_swap_layer_buffers},
    };

    for (const auto& handler : handler_array)
    {
        m_handler_func_ptr_map[handler.function] = handler.handler_func_ptr;
    }
}

Replay::StereoReplayEngineDLL::Win32CallExecutor::~Win32CallExecutor()
{
    /* The windows and the contexts belong to the replay thread: shutdown() has released them already. */
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::create_window_ex(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                        const IChunkSource&                          in_chunk_source,
                                                                        const bool&                                  in_is_wide,
                                                                        std::string*                                 out_error_ptr)
{
    constexpr size_t N_ARGUMENTS         = 12;
    constexpr size_t N_ARGUMENT_EX_STYLE = 0;
    constexpr size_t N_ARGUMENT_HEIGHT   = 7;
    constexpr size_t N_ARGUMENT_PARENT   = 8;
    constexpr size_t N_ARGUMENT_STYLE    = 3;
    constexpr size_t N_ARGUMENT_WIDTH    = 6;
    constexpr size_t N_ARGUMENT_X        = 4;
    constexpr size_t N_ARGUMENT_Y        = 5;

    std::wstring    class_name      = DEFAULT_WINDOW_CLASS_NAME;
    int             creation_height = 0;
    DWORD           creation_style  = 0;
    int             creation_width  = 0;
    bool            is_reused       = false;
    HWND            live_parent     = nullptr;
    HWND            live_window     = nullptr;
    bool            result          = false;
    WindowSignature signature;
    std::wstring    window_name;

    if (in_call.arg_in_vec.size() < N_ARGUMENTS)
    {
        *out_error_ptr = "CreateWindowEx: too few arguments.";

        goto end;
    }

    /* A class is told by a name, or by an atom, which means nothing to another process. */
    if (in_call.arg_in_vec.at(1).type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        const auto chunk_ptr = in_chunk_source.get_data_chunk_ptr(in_call.arg_in_vec.at(1).get_data_chunk_id() );

        if (chunk_ptr          != nullptr &&
            chunk_ptr->empty() == false)
        {
            class_name = get_wide_string(*chunk_ptr,
                                         in_is_wide);
        }
    }

    {
        const auto chunk_ptr = in_chunk_source.get_data_chunk_ptr(in_call.arg_in_vec.at(2).get_data_chunk_id() );

        if (chunk_ptr          != nullptr &&
            chunk_ptr->empty() == false)
        {
            window_name = get_wide_string(*chunk_ptr,
                                          in_is_wide);
        }
    }

    /* A window whose parent the replay does not know (e.g. a message-only window of the system) is not the app's. */
    if (get_live_handle(ObjectKind::HWND,
                        in_call.arg_in_vec.at   (N_ARGUMENT_PARENT),
                        reinterpret_cast<void**>(&live_parent),
                        out_error_ptr) == false)
    {
        out_error_ptr->clear();

        result = true;

        goto end;
    }

    if (register_window_class(class_name) == false)
    {
        *out_error_ptr = "CreateWindowEx: cannot register the window class.";

        goto end;
    }

    signature.class_name = class_name;
    signature.ex_style   = in_call.arg_in_vec.at(N_ARGUMENT_EX_STYLE).get_u32();
    signature.height     = static_cast<int>     (in_call.arg_in_vec.at(N_ARGUMENT_HEIGHT).get_u32() );
    signature.style      = in_call.arg_in_vec.at(N_ARGUMENT_STYLE).get_u32();
    signature.title      = window_name;
    signature.width      = static_cast<int>(in_call.arg_in_vec.at(N_ARGUMENT_WIDTH).get_u32() );

    /* After a reset, the window which the replay already has is used again, so that it stays on the screen. */
    is_reused = take_parked_window(signature,
                                  &live_window);

    if (is_reused == false)
    {
        creation_height = signature.height;
        creation_style  = signature.style;
        creation_width  = signature.width;

        make_non_scalable(signature.ex_style,
                         &creation_style,
                         &creation_width,
                         &creation_height);

        live_window = ::CreateWindowExW(signature.ex_style,
                                        class_name.c_str(),
                                        window_name.c_str(),
                                        creation_style,
                                        static_cast<int>(in_call.arg_in_vec.at(N_ARGUMENT_X).get_u32() ),
                                        static_cast<int>(in_call.arg_in_vec.at(N_ARGUMENT_Y).get_u32() ),
                                        creation_width,
                                        creation_height,
                                        live_parent,
                                        nullptr, /* menus are not replayed */
                                        ::GetModuleHandleW(nullptr),
                                        nullptr);

        if (live_window == nullptr)
        {
            *out_error_ptr = "CreateWindowEx failed.";

            goto end;
        }
    }

    m_window_signature_map[live_window] = signature;

    ::SetPropW(live_window,
               EXECUTOR_PROPERTY_NAME,
               this);

    if (in_call.returned_value_ptr != nullptr)
    {
        m_remapper_ptr->add(ObjectKind::HWND,
                            in_call.returned_value_ptr->get_ptr_as_u64(),
                            reinterpret_cast<uint64_t>(live_window) );
    }

    /* The app shows its windows with ShowWindow(), which is not part of the dump: the windows of the app have a title
     * bar, or are visible from the start; the others belong to the system's libraries.
     */
    if (m_are_windows_visible                                  &&
        is_reused                                     == false &&
        (signature.style & WS_CHILD)                  == 0     &&
        (signature.style & (WS_CAPTION | WS_VISIBLE)) != 0)
    {
        place_window(live_window);

        ::ShowWindow(live_window,
                     SW_SHOWNORMAL);
    }

    result = true;

end:
    return result;
}

void Replay::StereoReplayEngineDLL::Win32CallExecutor::destroy_parked_windows()
{
    for (const auto& parked : m_parked_window_map)
    {
        ::DestroyWindow(parked.first);
    }

    m_parked_window_map.clear();
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::execute(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                               const IChunkSource&                          in_chunk_source,
                                                               std::string*                                 out_error_ptr)
{
    const auto handler_iterator = m_handler_func_ptr_map.find(in_call.func);
    bool       result           = false;

    if (handler_iterator == m_handler_func_ptr_map.end() )
    {
        *out_error_ptr = "The Win32 executor does not know the function.";

        goto end;
    }

    result = (this->*handler_iterator->second)(in_call,
                                               in_chunk_source,
                                               out_error_ptr);

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::get_live_handle(const ObjectKind&                          in_kind,
                                                                       const APIInterceptor::APIFunctionArgument& in_argument,
                                                                       void**                                     out_live_handle_ptr,
                                                                       std::string*                               out_error_ptr) const
{
    uint64_t live_value = 0;
    bool     result     = m_remapper_ptr->get(in_kind,
                                              in_argument.get_ptr_as_u64(),
                                             &live_value);

    if (result)
    {
        *out_live_handle_ptr = reinterpret_cast<void*>(live_value);
    }
    else
    {
        *out_error_ptr = "A handle which the replay has not created is used.";
    }

    return result;
}

void* Replay::StereoReplayEngineDLL::Win32CallExecutor::get_wgl_function_ptr(const HDC&  in_hdc,
                                                                             const char* in_name_ptr) const
{
    void* result_ptr = reinterpret_cast<void*>(::wglGetProcAddress(in_name_ptr) );

    /* The functions can be asked for only while a context is current: use a throw-away one. */
    if (result_ptr == nullptr)
    {
        const HGLRC current_context = ::wglGetCurrentContext();

        if (current_context == nullptr)
        {
            const HGLRC temporary_context = ::wglCreateContext(in_hdc);

            if (temporary_context != nullptr)
            {
                ::wglMakeCurrent(in_hdc,
                                 temporary_context);

                result_ptr = reinterpret_cast<void*>(::wglGetProcAddress(in_name_ptr) );

                ::wglMakeCurrent  (nullptr,
                                   nullptr);
                ::wglDeleteContext(temporary_context);
            }
        }
    }

    return result_ptr;
}

std::wstring Replay::StereoReplayEngineDLL::Win32CallExecutor::get_wide_string(const Capture::APIDumpLoader::DataChunk& in_chunk,
                                                                               const bool&                              in_is_wide)
{
    std::wstring result;

    if (in_is_wide)
    {
        const wchar_t* characters_ptr = reinterpret_cast<const wchar_t*>(in_chunk.data() );

        result.assign(characters_ptr,
                      in_chunk.size() / sizeof(wchar_t) );
    }
    else
    {
        const int n_characters = ::MultiByteToWideChar(CP_ACP,
                                                       0, /* dwFlags */
                                                       reinterpret_cast<const char*>(in_chunk.data() ),
                                                       static_cast<int>(in_chunk.size() ),
                                                       nullptr,
                                                       0);

        result.resize(static_cast<size_t>(n_characters) );

        ::MultiByteToWideChar(CP_ACP,
                              0, /* dwFlags */
                              reinterpret_cast<const char*>(in_chunk.data() ),
                              static_cast<int>(in_chunk.size() ),
                              &result[0],
                              n_characters);
    }

    /* The chunk holds the terminator, too. */
    while (result.empty() == false &&
           result.back () == L'\0')
    {
        result.pop_back();
    }

    return result;
}

void Replay::StereoReplayEngineDLL::Win32CallExecutor::make_non_scalable(const DWORD& in_ex_style,
                                                                         DWORD*       inout_style_ptr,
                                                                         int*         inout_width_ptr,
                                                                         int*         inout_height_ptr)
{
    RECT        new_frame = {};
    const DWORD new_style = *inout_style_ptr & ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
    RECT        old_frame = {};

    /* The size of a window the user can pull is not the app's to keep: only a window with a title bar is made so. */
    if ((*inout_style_ptr & WS_CAPTION) != 0             &&
        (*inout_style_ptr & WS_CHILD)   == 0             &&
        *inout_width_ptr                != CW_USEDEFAULT &&
        *inout_height_ptr               != CW_USEDEFAULT)
    {
        ::AdjustWindowRectEx(&old_frame,
                             *inout_style_ptr,
                             FALSE,
                             in_ex_style);
        ::AdjustWindowRectEx(&new_frame,
                             new_style,
                             FALSE,
                             in_ex_style);

        /* The thin border takes less room: the window grows by the difference, and the area for drawing stays as big. */
        *inout_height_ptr += (new_frame.bottom - new_frame.top) - (old_frame.bottom - old_frame.top);
        *inout_width_ptr  += (new_frame.right  - new_frame.left) - (old_frame.right  - old_frame.left);
        *inout_style_ptr   =  new_style;
    }
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_copy_context(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       const IChunkSource&                          in_chunk_source,
                                                                       std::string*                                 out_error_ptr)
{
    HGLRC destination = nullptr;
    bool  result      = false;
    HGLRC source      = nullptr;

    (void) in_chunk_source;

    if (get_live_handle(ObjectKind::HGLRC,
                        in_call.arg_in_vec.at(0),
                        reinterpret_cast<void**>(&source),
                        out_error_ptr) == false)
    {
        goto end;
    }

    if (get_live_handle(ObjectKind::HGLRC,
                        in_call.arg_in_vec.at(1),
                        reinterpret_cast<void**>(&destination),
                        out_error_ptr) == false)
    {
        goto end;
    }

    ::wglCopyContext(source,
                     destination,
                     in_call.arg_in_vec.at(2).get_u32() );

    result = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_create_context(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                         const IChunkSource&                          in_chunk_source,
                                                                         std::string*                                 out_error_ptr)
{
    HDC   live_hdc = nullptr;
    HGLRC live_rc  = nullptr;
    bool  result   = false;

    (void) in_chunk_source;

    if (get_live_handle(ObjectKind::HDC,
                        in_call.arg_in_vec.at   (0),
                        reinterpret_cast<void**>(&live_hdc),
                        out_error_ptr)                      == false)
    {
        goto end;
    }

    live_rc = ::wglCreateContext(live_hdc);

    if (live_rc == nullptr)
    {
        *out_error_ptr = "wglCreateContext failed.";

        goto end;
    }

    m_context_set.insert(live_rc);

    if (in_call.returned_value_ptr != nullptr)
    {
        m_remapper_ptr->add(ObjectKind::HGLRC,
                            in_call.returned_value_ptr->get_ptr_as_u64(),
                            reinterpret_cast<uint64_t>(live_rc) );
    }

    result = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_create_context_attribs_arb(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                     const IChunkSource&                          in_chunk_source,
                                                                                     std::string*                                 out_error_ptr)
{
    const int*                        attrib_list_ptr = nullptr;
    PFNWGLCREATECONTEXTATTRIBSARBPROC create_func_ptr = nullptr;
    HDC                               live_hdc        = nullptr;
    HGLRC                             live_rc         = nullptr;
    HGLRC                             live_share      = nullptr;
    bool                              result          = false;

    if (get_live_handle(ObjectKind::HDC,
                        in_call.arg_in_vec.at(0),
                        reinterpret_cast<void**>(&live_hdc),
                        out_error_ptr) == false)
    {
        goto end;
    }

    if (get_live_handle(ObjectKind::HGLRC,
                        in_call.arg_in_vec.at(1),
                        reinterpret_cast<void**>(&live_share),
                        out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto chunk_ptr = in_chunk_source.get_data_chunk_ptr(in_call.arg_in_vec.at(2).get_data_chunk_id() );

        if (chunk_ptr          != nullptr &&
            chunk_ptr->empty() == false)
        {
            attrib_list_ptr = reinterpret_cast<const int*>(chunk_ptr->data() );
        }
    }

    create_func_ptr = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(get_wgl_function_ptr(live_hdc,
                                                                                               "wglCreateContextAttribsARB") );

    if (create_func_ptr == nullptr)
    {
        *out_error_ptr = "wglCreateContextAttribsARB is not available.";

        goto end;
    }

    live_rc = create_func_ptr(live_hdc,
                              live_share,
                              attrib_list_ptr);

    if (live_rc == nullptr)
    {
        *out_error_ptr = "wglCreateContextAttribsARB failed.";

        goto end;
    }

    m_context_set.insert(live_rc);

    if (in_call.returned_value_ptr != nullptr)
    {
        m_remapper_ptr->add(ObjectKind::HGLRC,
                            in_call.returned_value_ptr->get_ptr_as_u64(),
                            reinterpret_cast<uint64_t>(live_rc) );
    }

    result = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_create_layer_context(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                               const IChunkSource&                          in_chunk_source,
                                                                               std::string*                                 out_error_ptr)
{
    HDC   live_hdc = nullptr;
    HGLRC live_rc  = nullptr;
    bool  result   = false;

    (void) in_chunk_source;

    if (get_live_handle(ObjectKind::HDC,
                        in_call.arg_in_vec.at(0),
                        reinterpret_cast<void**>(&live_hdc),
                        out_error_ptr) == false)
    {
        goto end;
    }

    live_rc = ::wglCreateLayerContext(live_hdc,
                                      in_call.arg_in_vec.at(1).get_i32() );

    if (live_rc == nullptr)
    {
        *out_error_ptr = "wglCreateLayerContext failed.";

        goto end;
    }

    m_context_set.insert(live_rc);

    if (in_call.returned_value_ptr != nullptr)
    {
        m_remapper_ptr->add(ObjectKind::HGLRC,
                            in_call.returned_value_ptr->get_ptr_as_u64(),
                            reinterpret_cast<uint64_t>(live_rc) );
    }

    result = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_create_window_ex_a(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                             const IChunkSource&                          in_chunk_source,
                                                                             std::string*                                 out_error_ptr)
{
    return create_window_ex(in_call,
                            in_chunk_source,
                            false, /* in_is_wide */
                            out_error_ptr);
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_create_window_ex_w(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                             const IChunkSource&                          in_chunk_source,
                                                                             std::string*                                 out_error_ptr)
{
    return create_window_ex(in_call,
                            in_chunk_source,
                            true, /* in_is_wide */
                            out_error_ptr);
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_delete_context(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                         const IChunkSource&                          in_chunk_source,
                                                                         std::string*                                 out_error_ptr)
{
    HGLRC live_rc = nullptr;
    bool  result  = false;

    (void) in_chunk_source;

    if (get_live_handle(ObjectKind::HGLRC,
                        in_call.arg_in_vec.at(0),
                        reinterpret_cast<void**>(&live_rc),
                        out_error_ptr)                     == false)
    {
        goto end;
    }

    if (live_rc != nullptr)
    {
        ::wglDeleteContext(live_rc);

        m_context_set.erase(live_rc);
    }

    m_remapper_ptr->remove(ObjectKind::HGLRC,
                           in_call.arg_in_vec.at(0).get_ptr_as_u64() );

    result = true;

end:
    return result;
}

void Replay::StereoReplayEngineDLL::Win32CallExecutor::on_frame_presented()
{
    pump_messages();

    /* The first frame after a reset has been replayed: the windows of the app which it did not need are not wanted. */
    destroy_parked_windows();
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_get_dc(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                 const IChunkSource&                          in_chunk_source,
                                                                 std::string*                                 out_error_ptr)
{
    HDC  live_hdc  = nullptr;
    HWND live_hwnd = nullptr;
    bool result    = false;

    (void) in_chunk_source;

    /* GetDC(NULL) is the driver's: it asks for the screen. */
    if (in_call.arg_in_vec.at(0).get_ptr_as_u64() == 0)
    {
        result = true;

        goto end;
    }

    if (get_live_handle(ObjectKind::HWND,
                        in_call.arg_in_vec.at(0),
                        reinterpret_cast<void**>(&live_hwnd),
                        out_error_ptr) == false)
    {
        goto end;
    }

    live_hdc = ::GetDC(live_hwnd);

    if (live_hdc                   != nullptr &&
        in_call.returned_value_ptr != nullptr)
    {
        m_remapper_ptr->add(ObjectKind::HDC,
                            in_call.returned_value_ptr->get_ptr_as_u64(),
                            reinterpret_cast<uint64_t>(live_hdc) );
    }

    result = true;

end:
    return result;
}

void Replay::StereoReplayEngineDLL::Win32CallExecutor::on_idle()
{
    pump_messages();
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_ignored_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       const IChunkSource&                          in_chunk_source,
                                                                       std::string*                                 out_error_ptr)
{
    (void) in_call;
    (void) in_chunk_source;
    (void) out_error_ptr;

    return true;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_make_current(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       const IChunkSource&                          in_chunk_source,
                                                                       std::string*                                 out_error_ptr)
{
    HDC   live_hdc = nullptr;
    HGLRC live_rc  = nullptr;
    bool  result   = false;

    (void) in_chunk_source;

    if (get_live_handle(ObjectKind::HDC,
                        in_call.arg_in_vec.at(0),
                        reinterpret_cast<void**>(&live_hdc),
                        out_error_ptr) == false)
    {
        goto end;
    }

    if (get_live_handle(ObjectKind::HGLRC,
                        in_call.arg_in_vec.at(1),
                        reinterpret_cast<void**>(&live_rc),
                        out_error_ptr) == false)
    {
        goto end;
    }

    if (::wglMakeCurrent(live_hdc,
                         live_rc) == FALSE)
    {
        *out_error_ptr = "wglMakeCurrent failed.";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_release_dc(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                     const IChunkSource&                          in_chunk_source,
                                                                     std::string*                                 out_error_ptr)
{
    uint64_t live_hdc  = 0;
    uint64_t live_hwnd = 0;
    bool     result    = true;

    (void) in_chunk_source;
    (void) out_error_ptr;

    /* Neither the driver's calls (no window), nor a DC which the replay has not got are replayed. */
    if (m_remapper_ptr->get(ObjectKind::HWND,
                            in_call.arg_in_vec.at(0).get_ptr_as_u64(),
                           &live_hwnd) == false)
    {
        goto end;
    }

    if (m_remapper_ptr->get(ObjectKind::HDC,
                            in_call.arg_in_vec.at(1).get_ptr_as_u64(),
                           &live_hdc) == false)
    {
        goto end;
    }

    if (live_hwnd == 0)
    {
        goto end;
    }

    ::ReleaseDC(reinterpret_cast<HWND>(live_hwnd),
                reinterpret_cast<HDC>(live_hdc) );

    m_remapper_ptr->remove(ObjectKind::HDC,
                           in_call.arg_in_vec.at(1).get_ptr_as_u64() );

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_set_pixel_format(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                           const IChunkSource&                          in_chunk_source,
                                                                           std::string*                                 out_error_ptr)
{
    int                   live_format = 0;
    HDC                   live_hdc    = nullptr;
    PIXELFORMATDESCRIPTOR pixel_format_descriptor;
    bool                  result = false;

    if (get_live_handle(ObjectKind::HDC,
                        in_call.arg_in_vec.at(0),
                        reinterpret_cast<void**>(&live_hdc),
                        out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto chunk_ptr = in_chunk_source.get_data_chunk_ptr(in_call.arg_in_vec.at(2).get_data_chunk_id() );

        if (chunk_ptr         == nullptr                         ||
            chunk_ptr->size() <  sizeof(pixel_format_descriptor) )
        {
            *out_error_ptr = "SetPixelFormat: the pixel format descriptor was not dumped.";

            goto end;
        }

        ::memcpy(&pixel_format_descriptor,
                 chunk_ptr->data(),
                 sizeof(pixel_format_descriptor) );
    }

    /* The format numbers differ between systems: ask for the format that the dumped process got. */
    live_format = ::ChoosePixelFormat(live_hdc,
                                     &pixel_format_descriptor);

    if (live_format == 0)
    {
        *out_error_ptr = "ChoosePixelFormat failed.";

        goto end;
    }

    ::SetPixelFormat(live_hdc,
                     live_format,
                    &pixel_format_descriptor);

    result = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_share_lists(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                      const IChunkSource&                          in_chunk_source,
                                                                      std::string*                                 out_error_ptr)
{
    HGLRC first  = nullptr;
    bool  result = false;
    HGLRC second = nullptr;

    (void) in_chunk_source;

    if (get_live_handle(ObjectKind::HGLRC,
                        in_call.arg_in_vec.at   (0),
                        reinterpret_cast<void**>(&first),
                        out_error_ptr) == false)
    {
        goto end;
    }

    if (get_live_handle(ObjectKind::HGLRC,
                        in_call.arg_in_vec.at   (1),
                        reinterpret_cast<void**>(&second),
                        out_error_ptr) == false)
    {
        goto end;
    }

    ::wglShareLists(first,
                    second);

    result = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_swap_buffers(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       const IChunkSource&                          in_chunk_source,
                                                                       std::string*                                 out_error_ptr)
{
    HDC  live_hdc = nullptr;
    bool result   = false;

    (void) in_chunk_source;

    if (get_live_handle(ObjectKind::HDC,
                        in_call.arg_in_vec.at   (0),
                        reinterpret_cast<void**>(&live_hdc),
                        out_error_ptr) == false)
    {
        goto end;
    }

    ::SwapBuffers(live_hdc);

    result = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_swap_interval_ext(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                            const IChunkSource&                          in_chunk_source,
                                                                            std::string*                                 out_error_ptr)
{
    const HDC                 live_hdc      = ::wglGetCurrentDC();
    bool                      result        = true;
    PFNWGLSWAPINTERVALEXTPROC swap_func_ptr = nullptr;

    (void) in_chunk_source;
    (void) out_error_ptr;

    /* Without a current context, there is nothing to set the interval of. */
    if (live_hdc != nullptr)
    {
        swap_func_ptr = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(get_wgl_function_ptr(live_hdc,
                                                                                         "wglSwapIntervalEXT") );

        if (swap_func_ptr != nullptr)
        {
            swap_func_ptr(in_call.arg_in_vec.at(0).get_i32() );
        }
    }

    return result;
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::on_swap_layer_buffers(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                             const IChunkSource&                          in_chunk_source,
                                                                             std::string*                                 out_error_ptr)
{
    HDC  live_hdc = nullptr;
    bool result   = false;

    (void) in_chunk_source;

    if (get_live_handle(ObjectKind::HDC,
                        in_call.arg_in_vec.at(0),
                        reinterpret_cast<void**>(&live_hdc),
                        out_error_ptr) == false)
    {
        goto end;
    }

    ::wglSwapLayerBuffers(live_hdc,
                          in_call.arg_in_vec.at(1).get_u32() );

    result = true;

end:
    return result;
}

void Replay::StereoReplayEngineDLL::Win32CallExecutor::place_window(const HWND& in_window)
{
    constexpr int N_PARTS_TO_CENTER = 2;

    int             center_x   = 0;
    int             center_y   = 0;
    RECT            outer_rect = {};
    WindowPlacement placement;
    RECT            work_area  = {};

    ::GetWindowRect(in_window,
                   &outer_rect);

    placement.height = outer_rect.bottom - outer_rect.top;
    placement.width  = outer_rect.right  - outer_rect.left;

    /* A window which the replay makes anew takes the place of the one before it, wherever the user left it; the first
     * one is put in the middle of the screen.
     */
    if (m_has_last_placement)
    {
        const POINT last_center = {m_last_placement.x + m_last_placement.width  / N_PARTS_TO_CENTER,
                                   m_last_placement.y + m_last_placement.height / N_PARTS_TO_CENTER};
        MONITORINFO monitor_info = {};

        monitor_info.cbSize = sizeof(monitor_info);

        ::GetMonitorInfoW(::MonitorFromPoint(last_center,
                                             MONITOR_DEFAULTTONEAREST),
                         &monitor_info);

        work_area = monitor_info.rcWork;
        center_x  = last_center.x;
        center_y  = last_center.y;
    }
    else
    {
        ::SystemParametersInfoW(SPI_GETWORKAREA,
                                0,
                               &work_area,
                                0);

        center_x = work_area.left + (work_area.right  - work_area.left) / N_PARTS_TO_CENTER;
        center_y = work_area.top  + (work_area.bottom - work_area.top)  / N_PARTS_TO_CENTER;
    }

    /* The window is kept in view; if it is too big for the screen, its top left corner is. */
    placement.x = std::max<int32_t>(work_area.left,
                                    std::min<int32_t>(center_x - placement.width / N_PARTS_TO_CENTER,
                                                      work_area.right - placement.width) );
    placement.y = std::max<int32_t>(work_area.top,
                                    std::min<int32_t>(center_y - placement.height / N_PARTS_TO_CENTER,
                                                      work_area.bottom - placement.height) );

    ::SetWindowPos(in_window,
                   nullptr,
                   placement.x,
                   placement.y,
                   0,
                   0,
                   SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);

    if (m_window_placed_func != nullptr)
    {
        m_window_placed_func(placement);
    }
}

void Replay::StereoReplayEngineDLL::Win32CallExecutor::pump_messages()
{
    MSG message;

    while (::PeekMessageW(&message,
                          nullptr,
                          0, /* wMsgFilterMin */
                          0, /* wMsgFilterMax */
                          PM_REMOVE) )
    {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::register_window_class(const std::wstring& in_name)
{
    bool        result       = true;
    WNDCLASSEXW window_class = {};

    if (m_registered_class_name_set.find(in_name) == m_registered_class_name_set.end() )
    {
        window_class.cbSize        = sizeof            (window_class);
        window_class.hCursor       = ::LoadCursorW     (nullptr,
                                                        reinterpret_cast<LPCWSTR>(IDC_ARROW) );
        window_class.hInstance     = ::GetModuleHandleW(nullptr);
        window_class.lpfnWndProc   = &Win32CallExecutor::window_proc;
        window_class.lpszClassName = in_name.c_str();

        /* An OpenGL window must keep its DC, so that the pixel format sticks. The replay ignores WM_CLOSE: no button. */
        window_class.style         = CS_HREDRAW | CS_NOCLOSE | CS_OWNDC | CS_VREDRAW;

        result = (::RegisterClassExW(&window_class) != 0                           ||
                  ::GetLastError    ()              == ERROR_CLASS_ALREADY_EXISTS);

        if (result)
        {
            m_registered_class_name_set.insert(in_name);
        }
    }

    return result;
}

void Replay::StereoReplayEngineDLL::Win32CallExecutor::reset()
{
    ::wglMakeCurrent(nullptr,
                     nullptr);

    for (const auto context : m_context_set)
    {
        ::wglDeleteContext(context);
    }

    /* Where the window of the app is now is where a window made anew goes. */
    for (const auto& window : m_window_signature_map)
    {
        const bool is_iconic    = (::IsIconic(window.first)  != FALSE);
        const bool is_top_level = (::GetParent(window.first) == nullptr);
        RECT       outer_rect   = {};

        if (is_top_level                                &&
            is_iconic                          == false &&
            (window.second.style & WS_CAPTION) != 0)
        {
            ::GetWindowRect(window.first,
                           &outer_rect);

            m_has_last_placement    = true;
            m_last_placement.height = outer_rect.bottom - outer_rect.top;
            m_last_placement.width  = outer_rect.right  - outer_rect.left;
            m_last_placement.x      = outer_rect.left;
            m_last_placement.y      = outer_rect.top;

            break;
        }
    }

    /* The windows of the app are kept, so that they stay on the screen: the replay of the first frame takes them
     * again. The others are closed.
     */
    for (const auto& window : m_window_signature_map)
    {
        if (::GetParent(window.first) == nullptr)
        {
            m_parked_window_map[window.first] = window.second;
        }
        else
        {
            ::DestroyWindow(window.first);
        }
    }

    m_context_set.clear         ();
    m_window_signature_map.clear();
}

void Replay::StereoReplayEngineDLL::Win32CallExecutor::set_windows_visible(const bool& in_are_visible)
{
    m_are_windows_visible = in_are_visible;
}

void Replay::StereoReplayEngineDLL::Win32CallExecutor::shutdown()
{
    reset();

    destroy_parked_windows();
}

bool Replay::StereoReplayEngineDLL::Win32CallExecutor::take_parked_window(const WindowSignature& in_signature,
                                                                          HWND*                  out_window_ptr)
{
    bool result = false;

    for (auto iterator  = m_parked_window_map.begin();
              iterator != m_parked_window_map.end  ();
            ++iterator)
    {
        const BOOL is_window = ::IsWindow(iterator->first);

        if (is_window != FALSE)
        {
            if (iterator->second.class_name == in_signature.class_name &&
                iterator->second.ex_style   == in_signature.ex_style   &&
                iterator->second.height     == in_signature.height     &&
                iterator->second.style      == in_signature.style      &&
                iterator->second.title      == in_signature.title      &&
                iterator->second.width      == in_signature.width)
            {
                *out_window_ptr = iterator->first;
                result          = true;

                m_parked_window_map.erase(iterator);

                break;
            }
        }
    }

    return result;
}

LRESULT CALLBACK Replay::StereoReplayEngineDLL::Win32CallExecutor::window_proc(HWND   in_hwnd,
                                                                               UINT   in_message,
                                                                               WPARAM in_wparam,
                                                                               LPARAM in_lparam)
{
    LRESULT result = 0;

    /* A window the user closes would break the calls which follow; the replay destroys the windows itself, and only says that
     * the user asked.
     */
    if (in_message == WM_CLOSE)
    {
        const auto executor_ptr = static_cast<Win32CallExecutor*>(::GetPropW(in_hwnd,
                                                                             EXECUTOR_PROPERTY_NAME) );

        if (executor_ptr                       != nullptr &&
            executor_ptr->m_window_closed_func != nullptr)
        {
            executor_ptr->m_window_closed_func();
        }
    }
    else
    {
        if (in_message == WM_NCDESTROY)
        {
            ::RemovePropW(in_hwnd,
                          EXECUTOR_PROPERTY_NAME);
        }

        result = ::DefWindowProcW(in_hwnd,
                                  in_message,
                                  in_wparam,
                                  in_lparam);
    }

    return result;
}

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/ui/imgui_player_window_base.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <algorithm>
#include <cmath>
#include <d3d11.h>
#include <dwmapi.h>
#include <KnownFolders.h>
#include <ShlObj.h>

/* The font size for windows that ask for the system font (Segoe UI, found in the Windows fonts folder). */
static constexpr float SYSTEM_FONT_SIZE = 18.0f;

static const BOOL IS_DARK_TITLE_BAR = TRUE;

/* How far from the edge of a window with a title bar of its own the window is resized from, at 100% scale. */
static constexpr float RESIZE_BORDER_WIDTH = 6.0f;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

/* The ImGui Win32 backend declares this function only inside a disabled block, so it is declared here. */
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND   in_window,
                                                             UINT   in_message,
                                                             WPARAM in_wparam,
                                                             LPARAM in_lparam);

Utils::UI::ImGuiPlayerWindowBase::ImGuiPlayerWindowBase(const WindowOptions& in_options)
    :m_buttons_bottom        (0),
     m_buttons_left          (0),
     m_caption_height        (0),
     m_content_height        (0),
     m_content_width         (0),
     m_device_context_ptr    (nullptr),
     m_device_ptr            (nullptr),
     m_hwnd                  (nullptr),
     m_is_imgui_ready        (false),
     m_options               (in_options),
     m_render_target_view_ptr(nullptr),
     m_swap_chain_ptr        (nullptr)
{
    /* Stub */
}

Utils::UI::ImGuiPlayerWindowBase::~ImGuiPlayerWindowBase()
{
    if (m_is_imgui_ready)
    {
        ImGui_ImplDX11_Shutdown ();
        ImGui_ImplWin32_Shutdown();

        ImGui::DestroyContext();
    }

    destroy_render_target();

    if (m_swap_chain_ptr != nullptr)
    {
        m_swap_chain_ptr->Release();
    }

    if (m_device_context_ptr != nullptr)
    {
        m_device_context_ptr->Release();
    }

    if (m_device_ptr != nullptr)
    {
        m_device_ptr->Release();
    }

    if (m_hwnd != nullptr)
    {
        ::DestroyWindow(m_hwnd);
    }

    {
        const HMODULE module_handle = ::GetModuleHandleW(nullptr);

        ::UnregisterClassW(m_options.class_name.c_str(),
                           module_handle);
    }
}

bool Utils::UI::ImGuiPlayerWindowBase::create(std::string* out_error_ptr)
{
    constexpr POINT ORIGIN = {0, 0};

    RECT          client_rect   = {};
    const HMODULE module_handle = ::GetModuleHandleW(nullptr);
    bool          result        = false;
    float         scale         = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(ORIGIN,
                                                                                           MONITOR_DEFAULTTOPRIMARY) );
    WNDCLASSEXW   window_class  = {};
    DWORD         window_style  = (m_options.has_custom_title_bar) ? WS_SYSMENU
                                                                    : WS_CAPTION | WS_SYSMENU;

    if (m_options.is_resizable)
    {
        window_style |= WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME;
    }

    client_rect.bottom = static_cast<LONG>(m_options.client_height * scale);
    client_rect.right  = static_cast<LONG>(m_options.client_width  * scale);

    window_class.cbSize        = sizeof(window_class);
    window_class.hCursor       = ::LoadCursorA(nullptr,
                                               IDC_ARROW);
    window_class.hInstance     = module_handle;
    window_class.lpfnWndProc   = &ImGuiPlayerWindowBase::window_proc;
    window_class.lpszClassName = m_options.class_name.c_str();

    if (::RegisterClassExW(&window_class) == 0)
    {
        *out_error_ptr = "The class of the window cannot be registered.";

        goto end;
    }

    /* A window with a title bar of its own is all client area. */
    if (m_options.has_custom_title_bar == false)
    {
        ::AdjustWindowRect(&client_rect,
                           window_style,
                           FALSE);
    }

    m_hwnd = ::CreateWindowExW((m_options.is_topmost) ? WS_EX_TOPMOST
                                                      : 0,
                               m_options.class_name.c_str(),
                               nullptr, /* the title is set below */
                               window_style,
                               CW_USEDEFAULT,
                               CW_USEDEFAULT,
                               client_rect.right  - client_rect.left,
                               client_rect.bottom - client_rect.top,
                               nullptr,
                               nullptr,
                               module_handle,
                               this);

    if (m_hwnd == nullptr)
    {
        *out_error_ptr = "The window cannot be created.";

        goto end;
    }

    ::SetWindowTextA(m_hwnd,
                     m_options.title.c_str() );

    if (m_options.has_custom_title_bar)
    {
        /* One pixel of the frame is taken into the window: it keeps the shadow of the window, which a window without a title bar loses otherwise. The frame is
         * recalculated, so that the window is all client area from the start (see WM_NCCALCSIZE).
         */
        const MARGINS margins = {0, 0, 1, 0};

        ::DwmExtendFrameIntoClientArea(m_hwnd,
                                      &margins);
        ::SetWindowPos                (m_hwnd,
                                       nullptr,
                                       0,
                                       0,
                                       0,
                                       0,
                                       SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }

    /* The title bar is made dark to match the window. On a system that does not know the attribute, nothing happens. */
    ::DwmSetWindowAttribute(m_hwnd,
                            DWMWA_USE_IMMERSIVE_DARK_MODE,
                           &IS_DARK_TITLE_BAR,
                            sizeof(IS_DARK_TITLE_BAR) );

    if (create_device() == false)
    {
        *out_error_ptr = "The Direct3D 11 device of the window cannot be created.";

        goto end;
    }

    create_render_target();

    ImGui::CreateContext();

    ImGui::GetIO().IniFilename = nullptr;

    ImGui::StyleColorsDark();

    if (m_options.uses_system_font)
    {
        /* Without the font file, the window keeps the default ImGui font. */
        const std::string font_path = get_system_font_path();

        if (font_path.empty() == false)
        {
            ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path.c_str(),
                                                     SYSTEM_FONT_SIZE);
        }
    }

    /* Sizes and text are scaled to the DPI of the monitor. */
    ImGui::GetStyle().ScaleAllSizes(scale);

    ImGui::GetStyle().FontScaleDpi = scale;

    m_is_imgui_ready = true;

    if (ImGui_ImplWin32_Init(m_hwnd) == false)
    {
        *out_error_ptr = "ImGui cannot be set up for the window.";

        goto end;
    }

    if (ImGui_ImplDX11_Init(m_device_ptr,
                            m_device_context_ptr) == false)
    {
        *out_error_ptr = "ImGui cannot be set up for the window.";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Utils::UI::ImGuiPlayerWindowBase::create_device()
{
    constexpr UINT N_BUFFERS = 2;

    D3D_FEATURE_LEVEL       feature_level         = D3D_FEATURE_LEVEL_10_0;
    const D3D_FEATURE_LEVEL feature_level_array[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    bool                    result                = false;
    DXGI_SWAP_CHAIN_DESC    swap_chain_desc       = {};

    swap_chain_desc.BufferCount       = N_BUFFERS;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.OutputWindow      = m_hwnd;
    swap_chain_desc.SampleDesc.Count  = 1;
    swap_chain_desc.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;
    swap_chain_desc.Windowed          = TRUE;

    result = SUCCEEDED(::D3D11CreateDeviceAndSwapChain(nullptr, /* adapter */
                                                       D3D_DRIVER_TYPE_HARDWARE,
                                                       nullptr, /* software module */
                                                       0,       /* flags           */
                                                       feature_level_array,
                                                       static_cast<UINT>(sizeof(feature_level_array) / sizeof(feature_level_array[0]) ),
                                                       D3D11_SDK_VERSION,
                                                      &swap_chain_desc,
                                                      &m_swap_chain_ptr,
                                                      &m_device_ptr,
                                                      &feature_level,
                                                      &m_device_context_ptr) );

    return result;
}

void Utils::UI::ImGuiPlayerWindowBase::create_render_target()
{
    ID3D11Texture2D* back_buffer_ptr = nullptr;

    if (SUCCEEDED(m_swap_chain_ptr->GetBuffer(0,
                                              IID_PPV_ARGS(&back_buffer_ptr) )) )
    {
        m_device_ptr->CreateRenderTargetView(back_buffer_ptr,
                                             nullptr,
                                            &m_render_target_view_ptr);

        back_buffer_ptr->Release();
    }
}

void Utils::UI::ImGuiPlayerWindowBase::destroy_render_target()
{
    if (m_render_target_view_ptr != nullptr)
    {
        m_render_target_view_ptr->Release();

        m_render_target_view_ptr = nullptr;
    }
}

void Utils::UI::ImGuiPlayerWindowBase::enable_dpi_awareness()
{
    ImGui_ImplWin32_EnableDpiAwareness();
}

void Utils::UI::ImGuiPlayerWindowBase::extend_content(ContentExtent* inout_extent_ptr)
{
    inout_extent_ptr->bottom = std::max<float>(inout_extent_ptr->bottom,
                                               ImGui::GetItemRectMax().y);
    inout_extent_ptr->right  = std::max<float>(inout_extent_ptr->right,
                                               ImGui::GetItemRectMax().x);
}

void Utils::UI::ImGuiPlayerWindowBase::fit_to_content()
{
    RECT client_rect = {};

    ::GetClientRect(m_hwnd,
                   &client_rect);

    if (m_content_height > 0 &&
        m_content_width  > 0 &&
        (client_rect.bottom != m_content_height ||
         client_rect.right  != m_content_width) )
    {
        const SIZE outer_size = get_outer_size(m_content_width,
                                               m_content_height);

        ::SetWindowPos(m_hwnd,
                       nullptr,
                       0,
                       0,
                       outer_size.cx,
                       outer_size.cy,
                       SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);

        on_resized_to_content();
    }
}

ID3D11DeviceContext* Utils::UI::ImGuiPlayerWindowBase::get_device_context_ptr() const
{
    return m_device_context_ptr;
}

ID3D11Device* Utils::UI::ImGuiPlayerWindowBase::get_device_ptr() const
{
    return m_device_ptr;
}

LRESULT Utils::UI::ImGuiPlayerWindowBase::get_hit_test(const HWND&   in_window,
                                                       const LPARAM& in_lparam) const
{
    /* The edges of the window, which has no frame, are where it is resized from; the bar is where it is moved from; the rest is the window. */
    const UINT dpi          = ::GetDpiForWindow(in_window);
    const int  border       = static_cast<int>(RESIZE_BORDER_WIDTH * static_cast<float>(dpi) / 96.0f);
    POINT      point        = {static_cast<LONG>(static_cast<short>(LOWORD(in_lparam) ) ),
                               static_cast<LONG>(static_cast<short>(HIWORD(in_lparam) ) )};
    RECT       window_rect  = {};
    LRESULT    result       = HTCLIENT;

    ::GetWindowRect(in_window,
                   &window_rect);

    if (::IsZoomed(in_window) == FALSE)
    {
        /* Which of the three columns and the three rows of the window the point is in; the middle of both is the window itself. */
        constexpr LRESULT HIT_TEST_ARRAY[3][3] = {{HTTOPLEFT,    HTTOP,    HTTOPRIGHT},
                                                  {HTLEFT,       HTCLIENT, HTRIGHT},
                                                  {HTBOTTOMLEFT, HTBOTTOM, HTBOTTOMRIGHT}};

        const bool is_bottom = (point.y >= window_rect.bottom - border);
        const bool is_left   = (point.x <  window_rect.left   + border);
        const bool is_right  = (point.x >= window_rect.right  - border);
        const bool is_top    = (point.y <  window_rect.top    + border);
        const int  column    = (is_left)   ? 0
                                           : (is_right) ? 2
                                                        : 1;
        const int  row       = (is_top)    ? 0
                                           : (is_bottom) ? 2
                                                         : 1;

        result = HIT_TEST_ARRAY[row][column];

        if (result != HTCLIENT)
        {
            goto end;
        }
    }

    {
        const int x = point.x - window_rect.left;
        const int y = point.y - window_rect.top;

        if (y < m_caption_height &&
            (x < m_buttons_left  ||
             y >= m_buttons_bottom) )
        {
            result = HTCAPTION;
        }
    }

end:
    return result;
}

SIZE Utils::UI::ImGuiPlayerWindowBase::get_outer_size(const int& in_client_width,
                                                      const int& in_client_height) const
{
    RECT outer_rect = {0, 0, in_client_width, in_client_height};

    if (m_options.has_custom_title_bar == false)
    {
        ::AdjustWindowRectEx(&outer_rect,
                             static_cast<DWORD>(::GetWindowLongW(m_hwnd,
                                                                 GWL_STYLE) ),
                             FALSE,
                             static_cast<DWORD>(::GetWindowLongW(m_hwnd,
                                                                 GWL_EXSTYLE) ) );
    }

    return {outer_rect.right  - outer_rect.left,
            outer_rect.bottom - outer_rect.top};
}

std::string Utils::UI::ImGuiPlayerWindowBase::get_system_font_path()
{
    constexpr wchar_t FONT_FILE_NAME[] = L"\\segoeui.ttf";

    PWSTR       fonts_folder_ptr = nullptr;
    std::string result;

    /* The fonts folder is queried from Windows because Windows is not always installed in C:\Windows. */
    if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Fonts,
                                         0, /* dwFlags */
                                         nullptr,
                                        &fonts_folder_ptr) ) )
    {
        const std::wstring font_path = std::wstring(fonts_folder_ptr) + FONT_FILE_NAME;

        if (::GetFileAttributesW(font_path.c_str() ) != INVALID_FILE_ATTRIBUTES)
        {
            /* ImGui wants UTF-8. */
            const int n_bytes = ::WideCharToMultiByte(CP_UTF8,
                                                      0,
                                                      font_path.c_str(),
                                                      -1,
                                                      nullptr,
                                                      0,
                                                      nullptr,
                                                      nullptr);

            if (n_bytes > 1)
            {
                result.resize(static_cast<size_t>(n_bytes) - 1);

                ::WideCharToMultiByte(CP_UTF8,
                                      0,
                                      font_path.c_str(),
                                      -1,
                                     &result[0],
                                      n_bytes,
                                      nullptr,
                                      nullptr);
            }
        }
    }

    /* Safe even if the folder was not found, because the pointer is then null. */
    ::CoTaskMemFree(fonts_folder_ptr);

    return result;
}

HWND Utils::UI::ImGuiPlayerWindowBase::get_window() const
{
    return m_hwnd;
}

bool Utils::UI::ImGuiPlayerWindowBase::handle_message(const HWND&   in_window,
                                                      const UINT&   in_message,
                                                      const WPARAM& in_wparam,
                                                      const LPARAM& in_lparam,
                                                      LRESULT*      out_result_ptr)
{
    (void) in_window;
    (void) in_message;
    (void) in_wparam;
    (void) in_lparam;
    (void) out_result_ptr;

    return false;
}

bool Utils::UI::ImGuiPlayerWindowBase::is_window_maximized() const
{
    return (::IsZoomed(m_hwnd) != FALSE);
}

void Utils::UI::ImGuiPlayerWindowBase::minimize_window()
{
    ::ShowWindow(m_hwnd,
                 SW_MINIMIZE);
}

void Utils::UI::ImGuiPlayerWindowBase::on_resized_to_content()
{
    /* Stub */
}

LRESULT Utils::UI::ImGuiPlayerWindowBase::process_message(const HWND&   in_window,
                                                          const UINT&   in_message,
                                                          const WPARAM& in_wparam,
                                                          const LPARAM& in_lparam)
{
    LRESULT result = 0;

    if (ImGui_ImplWin32_WndProcHandler(in_window,
                                       in_message,
                                       in_wparam,
                                       in_lparam) != 0)
    {
        result = 1;

        goto end;
    }

    if (handle_message(in_window,
                       in_message,
                       in_wparam,
                       in_lparam,
                      &result) )
    {
        goto end;
    }

    switch (in_message)
    {
        case WM_DESTROY:
        {
            ::PostQuitMessage(0);

            break;
        }

        case WM_NCCALCSIZE:
        {
            /* The whole window is client area. A maximized window is larger than the screen by the width of its frame, so it is cut back by that. */
            if (m_options.has_custom_title_bar &&
                in_wparam                      != 0)
            {
                if (::IsZoomed(in_window) )
                {
                    NCCALCSIZE_PARAMS* params_ptr = reinterpret_cast<NCCALCSIZE_PARAMS*>(in_lparam);
                    const UINT         dpi        = ::GetDpiForWindow(in_window);
                    const int          padding    = ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER,
                                                                             dpi);
                    const int          frame_x    = ::GetSystemMetricsForDpi(SM_CXFRAME,
                                                                             dpi) + padding;
                    const int          frame_y    = ::GetSystemMetricsForDpi(SM_CYFRAME,
                                                                             dpi) + padding;

                    params_ptr->rgrc[0].bottom -= frame_y;
                    params_ptr->rgrc[0].left   += frame_x;
                    params_ptr->rgrc[0].right  -= frame_x;
                    params_ptr->rgrc[0].top    += frame_y;
                }

                goto end;
            }

            result = ::DefWindowProcW(in_window,
                                      in_message,
                                      in_wparam,
                                      in_lparam);

            break;
        }

        case WM_NCHITTEST:
        {
            if (m_options.has_custom_title_bar)
            {
                result = get_hit_test(in_window,
                                      in_lparam);
            }
            else
            {
                result = ::DefWindowProcW(in_window,
                                          in_message,
                                          in_wparam,
                                          in_lparam);
            }

            break;
        }

        case WM_SIZE:
        {
            if (in_wparam != SIZE_MINIMIZED)
            {
                resize(LOWORD(in_lparam),
                       HIWORD(in_lparam) );
            }

            break;
        }

        case WM_SYSCOMMAND:
        {
            /* Pressing Alt would open the system menu and take the keyboard away from the window. */
            if ((in_wparam & 0xfff0) == SC_KEYMENU)
            {
                goto end;
            }

            result = ::DefWindowProcW(in_window,
                                      in_message,
                                      in_wparam,
                                      in_lparam);

            break;
        }

        default:
        {
            result = ::DefWindowProcW(in_window,
                                      in_message,
                                      in_wparam,
                                      in_lparam);

            break;
        }
    }

end:
    return result;
}

void Utils::UI::ImGuiPlayerWindowBase::render_frame()
{
    const float clear_color_fp32vec4[] = {0.1f, 0.1f, 0.12f, 1.0f};

    /* Use the content size measured in the previous frame. */
    if (m_options.fits_content)
    {
        fit_to_content();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame();

    {
        ContentExtent     extent;
        const ImGuiStyle& style  = ImGui::GetStyle();

        ImGui::SetNextWindowPos (ImVec2(0.0f,
                                        0.0f) );
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

        ImGui::Begin("Contents",
                     nullptr,
                     ImGuiWindowFlags_NoCollapse      |
                     ImGuiWindowFlags_NoMove          |
                     ImGuiWindowFlags_NoResize        |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoTitleBar);

        draw_contents(&extent);

        ImGui::End();

        m_content_height = static_cast<int>(std::ceil(extent.bottom + style.WindowPadding.y) );
        m_content_width  = static_cast<int>(std::ceil(extent.right  + style.WindowPadding.x) );
    }

    ImGui::Render();

    m_device_context_ptr->OMSetRenderTargets(1,
                                            &m_render_target_view_ptr,
                                             nullptr);
    m_device_context_ptr->ClearRenderTargetView(m_render_target_view_ptr,
                                                clear_color_fp32vec4);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData() );

    m_swap_chain_ptr->Present(1,  /* sync interval */
                              0); /* flags         */
}

void Utils::UI::ImGuiPlayerWindowBase::request_close()
{
    ::PostMessageW(m_hwnd,
                   WM_CLOSE,
                   0,
                   0);
}

void Utils::UI::ImGuiPlayerWindowBase::set_title_bar_layout(const int& in_caption_height,
                                                            const int& in_buttons_left,
                                                            const int& in_buttons_bottom)
{
    m_buttons_bottom = in_buttons_bottom;
    m_buttons_left   = in_buttons_left;
    m_caption_height = in_caption_height;
}

void Utils::UI::ImGuiPlayerWindowBase::toggle_maximize()
{
    ::ShowWindow(m_hwnd,
                 (::IsZoomed(m_hwnd) ) ? SW_RESTORE
                                       : SW_MAXIMIZE);
}

void Utils::UI::ImGuiPlayerWindowBase::resize(const UINT& in_width,
                                              const UINT& in_height)
{
    if (m_swap_chain_ptr != nullptr)
    {
        destroy_render_target();

        m_swap_chain_ptr->ResizeBuffers(0,
                                        in_width,
                                        in_height,
                                        DXGI_FORMAT_UNKNOWN,
                                        0);

        create_render_target();
    }
}

void Utils::UI::ImGuiPlayerWindowBase::run()
{
    constexpr DWORD MINIMIZED_SLEEP_MS = 20;

    MSG message = {};

    /* Two frames are rendered before the window is shown, so that a window that fits its content has its final size when it appears. */
    render_frame();
    render_frame();

    ::ShowWindow(m_hwnd,
                 SW_SHOWNORMAL);

    while (message.message != WM_QUIT)
    {
        if (::PeekMessageW(&message,
                           nullptr,
                           0,
                           0,
                           PM_REMOVE) )
        {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);

            continue;
        }

        if (::IsIconic(m_hwnd) )
        {
            ::Sleep(MINIMIZED_SLEEP_MS);

            continue;
        }

        render_frame();
    }
}

LRESULT CALLBACK Utils::UI::ImGuiPlayerWindowBase::window_proc(HWND   in_window,
                                                               UINT   in_message,
                                                               WPARAM in_wparam,
                                                               LPARAM in_lparam)
{
    ImGuiPlayerWindowBase* window_ptr = nullptr;

    if (in_message == WM_NCCREATE)
    {
        window_ptr = static_cast<ImGuiPlayerWindowBase*>(reinterpret_cast<CREATESTRUCTW*>(in_lparam)->lpCreateParams);

        ::SetWindowLongPtrW(in_window,
                            GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(window_ptr) );
    }
    else
    {
        window_ptr = reinterpret_cast<ImGuiPlayerWindowBase*>(::GetWindowLongPtrW(in_window,
                                                                            GWLP_USERDATA) );
    }

    return (window_ptr != nullptr) ? window_ptr->process_message(in_window,
                                                                 in_message,
                                                                 in_wparam,
                                                                 in_lparam)
                                   : ::DefWindowProcW           (in_window,
                                                                 in_message,
                                                                 in_wparam,
                                                                 in_lparam);
}

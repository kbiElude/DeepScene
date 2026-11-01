/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "VR/UIHost/imgui_window_host.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <algorithm>
#include <cmath>
#include <d3d11.h>
#include <dwmapi.h>

/* The font of the windows which ask for the one of the system: Segoe UI, which every supported Windows has. */
static constexpr const char* SYSTEM_FONT_PATH = "C:/Windows/Fonts/segoeui.ttf";
static constexpr float       SYSTEM_FONT_SIZE = 18.0f;

static const BOOL IS_DARK_TITLE_BAR = TRUE;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

/* Declared by the backend only inside a disabled block. */
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND   in_window,
                                                             UINT   in_message,
                                                             WPARAM in_wparam,
                                                             LPARAM in_lparam);

VR::UIHost::ImGuiWindowHost::ImGuiWindowHost(const WindowOptions& in_options)
    :m_content_height        (0),
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

VR::UIHost::ImGuiWindowHost::~ImGuiWindowHost()
{
    if (m_is_imgui_ready)
    {
        ImGui_ImplDX11_Shutdown();
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

    ::UnregisterClassW(m_options.class_name.c_str(),
                       ::GetModuleHandleW(nullptr) );
}

bool VR::UIHost::ImGuiWindowHost::create(std::string* out_error_ptr)
{
    constexpr POINT ORIGIN = {0, 0};

    RECT          client_rect   = {};
    const HMODULE module_handle = ::GetModuleHandleW(nullptr);
    bool          result        = false;
    float         scale         = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(ORIGIN,
                                                                                         MONITOR_DEFAULTTOPRIMARY) );
    WNDCLASSEXW   window_class  = {};
    DWORD         window_style  = WS_CAPTION | WS_SYSMENU;

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
    window_class.lpfnWndProc   = &ImGuiWindowHost::window_proc;
    window_class.lpszClassName = m_options.class_name.c_str();

    if (::RegisterClassExW(&window_class) == 0)
    {
        *out_error_ptr = "The class of the window cannot be registered.";

        goto end;
    }

    ::AdjustWindowRect(&client_rect,
                       window_style,
                       FALSE);

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

    /* The title bar follows the dark window; where the system does not know the attribute, nothing happens. */
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
        ImGui::GetIO().Fonts->AddFontFromFileTTF(SYSTEM_FONT_PATH,
                                                 SYSTEM_FONT_SIZE);
    }

    /* The sizes and the text follow the DPI of the monitor. */
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

bool VR::UIHost::ImGuiWindowHost::create_device()
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

void VR::UIHost::ImGuiWindowHost::create_render_target()
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

void VR::UIHost::ImGuiWindowHost::destroy_render_target()
{
    if (m_render_target_view_ptr != nullptr)
    {
        m_render_target_view_ptr->Release();

        m_render_target_view_ptr = nullptr;
    }
}

void VR::UIHost::ImGuiWindowHost::enable_dpi_awareness()
{
    ImGui_ImplWin32_EnableDpiAwareness();
}

void VR::UIHost::ImGuiWindowHost::extend_content(ContentExtent* inout_extent_ptr)
{
    inout_extent_ptr->bottom = std::max<float>(inout_extent_ptr->bottom,
                                               ImGui::GetItemRectMax().y);
    inout_extent_ptr->right  = std::max<float>(inout_extent_ptr->right,
                                               ImGui::GetItemRectMax().x);
}

void VR::UIHost::ImGuiWindowHost::fit_to_content()
{
    RECT client_rect = {};
    RECT outer_rect  = {};

    ::GetClientRect(m_hwnd,
                   &client_rect);

    if (m_content_height > 0 &&
        m_content_width  > 0 &&
        (client_rect.bottom != m_content_height ||
         client_rect.right  != m_content_width) )
    {
        outer_rect.bottom = m_content_height;
        outer_rect.right  = m_content_width;

        ::AdjustWindowRectEx(&outer_rect,
                             static_cast<DWORD>(::GetWindowLongW(m_hwnd,
                                                                 GWL_STYLE) ),
                             FALSE,
                             static_cast<DWORD>(::GetWindowLongW(m_hwnd,
                                                                 GWL_EXSTYLE) ));

        ::SetWindowPos(m_hwnd,
                       nullptr,
                       0,
                       0,
                       outer_rect.right  - outer_rect.left,
                       outer_rect.bottom - outer_rect.top,
                       SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);

        on_resized_to_content();
    }
}

ID3D11DeviceContext* VR::UIHost::ImGuiWindowHost::get_device_context_ptr() const
{
    return m_device_context_ptr;
}

ID3D11Device* VR::UIHost::ImGuiWindowHost::get_device_ptr() const
{
    return m_device_ptr;
}

HWND VR::UIHost::ImGuiWindowHost::get_window() const
{
    return m_hwnd;
}

bool VR::UIHost::ImGuiWindowHost::handle_message(const HWND&   in_window,
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

void VR::UIHost::ImGuiWindowHost::on_resized_to_content()
{
    /* Stub */
}

LRESULT VR::UIHost::ImGuiWindowHost::process_message(const HWND&   in_window,
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
            /* The menu of the Alt key would take the keyboard from the window. */
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

void VR::UIHost::ImGuiWindowHost::render_frame()
{
    const float clear_color_fp32vec4[] = {0.1f, 0.1f, 0.12f, 1.0f};

    /* The size which the last frame needed. */
    if (m_options.fits_content)
    {
        fit_to_content();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame();

    {
        ContentExtent     extent;
        const ImGuiStyle& style = ImGui::GetStyle();

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

void VR::UIHost::ImGuiWindowHost::request_close()
{
    ::PostMessageW(m_hwnd,
                   WM_CLOSE,
                   0,
                   0);
}

void VR::UIHost::ImGuiWindowHost::resize(const UINT& in_width,
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

void VR::UIHost::ImGuiWindowHost::run()
{
    constexpr DWORD MINIMIZED_SLEEP_MS = 20;

    MSG message = {};

    /* A window which fits its content takes the size of it before it is on the screen. */
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

LRESULT CALLBACK VR::UIHost::ImGuiWindowHost::window_proc(HWND   in_window,
                                                          UINT   in_message,
                                                          WPARAM in_wparam,
                                                          LPARAM in_lparam)
{
    ImGuiWindowHost* window_ptr = nullptr;

    if (in_message == WM_NCCREATE)
    {
        window_ptr = static_cast<ImGuiWindowHost*>(reinterpret_cast<CREATESTRUCTW*>(in_lparam)->lpCreateParams);

        ::SetWindowLongPtrW(in_window,
                            GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(window_ptr) );
    }
    else
    {
        window_ptr = reinterpret_cast<ImGuiWindowHost*>(::GetWindowLongPtrW(in_window,
                                                                            GWLP_USERDATA) );
    }

    return (window_ptr != nullptr) ? window_ptr->process_message(in_window,
                                                                 in_message,
                                                                 in_wparam,
                                                                 in_lparam)
                                   : ::DefWindowProcW(in_window,
                                                      in_message,
                                                      in_wparam,
                                                      in_lparam);
}

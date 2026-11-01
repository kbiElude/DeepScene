/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "preview_window.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string.h>

static constexpr UINT           QUIT_MESSAGE      = WM_APP + 1;
static constexpr const wchar_t* WINDOW_CLASS_NAME = L"StereoReplayPlayerPreviewWindow";

/* One triangle which covers the viewport; the viewport is where the image goes. */
static constexpr const char* SHADER_SOURCE =
        "struct VsOut\n"
        "{\n"
        "\n"
        "    float4 position : SV_POSITION;\n"
        "    float2 uv       : TEXCOORD0;\n"
        "};\n"
        "VsOut vs_main(uint id : SV_VertexID)\n"
        "{\n"
        "    VsOut result;\n"
        "\n"
        "    float2 uv       = float2((id << 1) & 2,\n"
        "                             id & 2);\n"
        "    result.position = float4(uv * float2(2, -2) + float2(-1, 1),\n"
        "                             0,\n"
        "                             1);\n"
        "    result.uv       = uv;\n"
        "\n"
        "    return result;\n"
        "}\n"
        "Texture2D    g_texture : register(t0);\n"
        "SamplerState g_sampler : register(s0);\n"
        "\n"
        "float4 ps_main(VsOut in_vertex) : SV_TARGET\n"
        "{\n"
        "    return g_texture.Sample(g_sampler, in_vertex.uv);\n"
        "}\n";

Replay::StereoReplayPlayer::PreviewWindow::PreviewWindow(const EyeMode& in_eye_mode)
    :m_close_func            (),
     m_device_context_ptr    (nullptr),
     m_device_ptr            (nullptr),
     m_eye_mode              (in_eye_mode),
     m_first_frame           (0),
     m_hwnd                  (nullptr),
     m_image_buffer          (in_eye_mode),
     m_image_rate            (),
     m_is_visible            (true),
     m_last_frame            (UINT32_MAX),
     m_pixel_shader_ptr      (nullptr),
     m_render_target_view_ptr(nullptr),
     m_sampler_ptr           (nullptr),
     m_swap_chain_ptr        (nullptr),
     m_vertex_shader_ptr     (nullptr)
{
    /* Stub */
}

Replay::StereoReplayPlayer::PreviewWindow::~PreviewWindow()
{
    if (m_thread.joinable() )
    {
        /* The thread ends the message loop, and destroys the window. */
        ::PostMessageW(m_hwnd,
                       QUIT_MESSAGE,
                       0,  /* wparam */
                       0); /* lparam */

        m_thread.join();
    }
}

bool Replay::StereoReplayPlayer::PreviewWindow::create(std::string* out_error_ptr)
{
    const auto is_done_func = [this]
    {
        return m_startup.is_done;
    };
    bool       result       = false;

    m_thread = std::thread(&PreviewWindow::run_thread,
                           this);

    {
        std::unique_lock<std::mutex> lock(m_startup.mutex);

        m_startup.cv.wait(lock,
                          is_done_func);

        *out_error_ptr = m_startup.error;
        result         = m_startup.is_ok;
    }

    if (result == false)
    {
        m_thread.join();
    }

    return result;
}

bool Replay::StereoReplayPlayer::PreviewWindow::create_device(std::string* out_error_ptr)
{
    ID3DBlob*            error_blob_ptr  = nullptr;
    ID3DBlob*            pixel_blob_ptr  = nullptr;
    bool                 result          = false;
    ID3DBlob*            vertex_blob_ptr = nullptr;

    {
        DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};

        swap_chain_desc.BufferCount       = 1;
        swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.OutputWindow      = m_hwnd;
        swap_chain_desc.SampleDesc.Count  = 1;
        swap_chain_desc.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;
        swap_chain_desc.Windowed          = TRUE;

        if (FAILED(::D3D11CreateDeviceAndSwapChain(nullptr, /* adapter           */
                                                   D3D_DRIVER_TYPE_HARDWARE,
                                                   nullptr, /* software          */
                                                   0,       /* flags             */
                                                   nullptr, /* feature levels    */
                                                   0,       /* n feature levels  */
                                                   D3D11_SDK_VERSION,
                                                  &swap_chain_desc,
                                                  &m_swap_chain_ptr,
                                                  &m_device_ptr,
                                                   nullptr, /* feature level     */
                                                  &m_device_context_ptr) ) )
        {
            *out_error_ptr = "The Direct3D 11 device of the preview cannot be created.";

            goto end;
        }
    }

    if (FAILED(::D3DCompile(SHADER_SOURCE,
                            ::strlen(SHADER_SOURCE),
                            nullptr, /* source name */
                            nullptr, /* defines     */
                            nullptr, /* include     */
                            "vs_main",
                            "vs_4_0",
                            0,       /* flags 1     */
                            0,       /* flags 2     */
                           &vertex_blob_ptr,
                           &error_blob_ptr) ) )
    {
        *out_error_ptr = "The vertex shader of the preview cannot be compiled" + get_blob_text(error_blob_ptr);

        goto end;
    }

    if (FAILED(::D3DCompile(SHADER_SOURCE,
                            ::strlen(SHADER_SOURCE),
                            nullptr, /* source name */
                            nullptr, /* defines     */
                            nullptr, /* include     */
                            "ps_main",
                            "ps_4_0",
                            0,       /* flags 1     */
                            0,       /* flags 2     */
                           &pixel_blob_ptr,
                           &error_blob_ptr) ) )
    {
        *out_error_ptr = "The pixel shader of the preview cannot be compiled" + get_blob_text(error_blob_ptr);

        goto end;
    }

    if (FAILED(m_device_ptr->CreateVertexShader(vertex_blob_ptr->GetBufferPointer(),
                                                vertex_blob_ptr->GetBufferSize   (),
                                                nullptr,
                                               &m_vertex_shader_ptr) ) )
    {
        *out_error_ptr = "The vertex shader of the preview cannot be created.";

        goto end;
    }

    if (FAILED(m_device_ptr->CreatePixelShader(pixel_blob_ptr->GetBufferPointer(),
                                               pixel_blob_ptr->GetBufferSize   (),
                                               nullptr,
                                              &m_pixel_shader_ptr) ) )
    {
        *out_error_ptr = "The pixel shader of the preview cannot be created.";

        goto end;
    }

    {
        D3D11_SAMPLER_DESC   sampler_desc    = {};

        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

        if (FAILED(m_device_ptr->CreateSamplerState(&sampler_desc,
                                                    &m_sampler_ptr) ) )
        {
            *out_error_ptr = "The sampler of the preview cannot be created.";

            goto end;
        }
    }

    create_render_target();

    result = true;

end:
    release(&error_blob_ptr);
    release(&pixel_blob_ptr);
    release(&vertex_blob_ptr);

    return result;
}

void Replay::StereoReplayPlayer::PreviewWindow::create_render_target()
{
    ID3D11Texture2D* back_buffer_ptr = nullptr;

    if (SUCCEEDED(m_swap_chain_ptr->GetBuffer(0, /* buffer */
                                              __uuidof(ID3D11Texture2D),
                                              reinterpret_cast<void**>(&back_buffer_ptr) ) ) )
    {
        m_device_ptr->CreateRenderTargetView(back_buffer_ptr,
                                             nullptr,
                                            &m_render_target_view_ptr);

        back_buffer_ptr->Release();
    }
}

bool Replay::StereoReplayPlayer::PreviewWindow::create_window(std::string* out_error_ptr)
{
    constexpr int HEIGHT = 540;
    constexpr int WIDTH  = 1280;

    ATOM        class_atom   = 0;
    RECT        client_rect  = {0, 0, WIDTH, HEIGHT};
    DWORD       last_error   = 0;
    bool        result       = false;
    WNDCLASSEXW window_class = {};
    const DWORD window_style = WS_OVERLAPPEDWINDOW;

    window_class.cbSize        = sizeof            (window_class);
    window_class.hCursor       = ::LoadCursorA     (nullptr,
                                                    IDC_ARROW);
    window_class.hInstance     = ::GetModuleHandleW(nullptr);
    window_class.lpfnWndProc   = window_proc;
    window_class.lpszClassName = WINDOW_CLASS_NAME;

    class_atom = ::RegisterClassExW(&window_class);

    if (class_atom == 0)
    {
        last_error = ::GetLastError();

        if (last_error != ERROR_CLASS_ALREADY_EXISTS)
        {
            *out_error_ptr = "The class of the preview window cannot be registered.";

            goto end;
        }
    }

    ::AdjustWindowRect(&client_rect,
                        window_style,
                        FALSE /* bMenu */);

    m_hwnd = ::CreateWindowExW(0, /* extended style */
                               WINDOW_CLASS_NAME,
                               (m_eye_mode == EyeMode::BOTH) ? L"Preview: left | right"
                             : (m_eye_mode == EyeMode::LEFT) ? L"Preview: left"
                                                             : L"Preview: right",
                               window_style,
                               CW_USEDEFAULT,
                               CW_USEDEFAULT,
                               client_rect.right  - client_rect.left,
                               client_rect.bottom - client_rect.top,
                               nullptr, /* parent */
                               nullptr, /* menu   */
                               window_class.hInstance,
                               this);

    if (m_hwnd == nullptr)
    {
        *out_error_ptr = "The preview window cannot be created.";

        goto end;
    }

    if (create_device(out_error_ptr) == false)
    {
        goto end;
    }

    ::ShowWindow(m_hwnd,
                 SW_SHOWNOACTIVATE);

    result = true;

end:
    return result;
}

void Replay::StereoReplayPlayer::PreviewWindow::destroy_render_target()
{
    release(&m_render_target_view_ptr);
}

void Replay::StereoReplayPlayer::PreviewWindow::destroy_resources()
{
    for (auto& texture : m_eye_texture)
    {
        release(&texture.view_ptr);
        release(&texture.tex_ptr);
    }

    destroy_render_target();

    release(&m_sampler_ptr);
    release(&m_pixel_shader_ptr);
    release(&m_vertex_shader_ptr);
    release(&m_swap_chain_ptr);
    release(&m_device_context_ptr);
    release(&m_device_ptr);

    if (m_hwnd != nullptr)
    {
        ::DestroyWindow(m_hwnd);
    }
}

void Replay::StereoReplayPlayer::PreviewWindow::draw_eye(const EyeTexture& in_texture,
                                                         const RECT&       in_cell)
{
    constexpr UINT N_TRIANGLE_VERTICES = 3;

    if (in_texture.view_ptr != nullptr)
    {
        const float    cell_height = static_cast<float>(in_cell.bottom - in_cell.top);
        const float    cell_width  = static_cast<float>(in_cell.right  - in_cell.left);
        const float    scale_x     = cell_width  / static_cast<float>(in_texture.width);
        const float    scale_y     = cell_height / static_cast<float>(in_texture.height);
        const float    scale       = (scale_x < scale_y) ? scale_x
                                                         : scale_y;
        D3D11_VIEWPORT viewport    = {};

        /* The image keeps its proportions, and sits in the middle of the cell. */
        viewport.Height   = static_cast<float>(in_texture.height) * scale;
        viewport.MaxDepth = 1.0f;
        viewport.Width    = static_cast<float>(in_texture.width)  * scale;
        viewport.TopLeftX = static_cast<float>(in_cell.left) + (cell_width  - viewport.Width)  * 0.5f;
        viewport.TopLeftY = static_cast<float>(in_cell.top)  + (cell_height - viewport.Height) * 0.5f;

        m_device_context_ptr->RSSetViewports      (1, /* n viewports */
                                                  &viewport);
        m_device_context_ptr->PSSetShaderResources(0, /* start slot */
                                                   1, /* n views    */
                                                  &in_texture.view_ptr);
        m_device_context_ptr->Draw                (N_TRIANGLE_VERTICES, /* n vertices    */
                                                   0);                  /* start vertex  */
    }
}

std::string Replay::StereoReplayPlayer::PreviewWindow::get_blob_text(ID3DBlob* in_blob_ptr)
{
    return (in_blob_ptr != nullptr) ? ": " + std::string(static_cast<const char*>  (in_blob_ptr->GetBufferPointer() ),
                                                         in_blob_ptr->GetBufferSize() )
                                    : ".";
}

LRESULT Replay::StereoReplayPlayer::PreviewWindow::handle_message(const HWND&   in_window,
                                                                  const UINT&   in_message,
                                                                  const WPARAM& in_wparam,
                                                                  const LPARAM& in_lparam)
{
    LRESULT result = 0;

    switch (in_message)
    {
        case WM_APP:
        {
            render();

            break;
        }

        /* The preview is hidden, and the player is told to end. */
        case WM_CLOSE:
        {
            m_is_visible = false;

            ::ShowWindow(in_window,
                         SW_HIDE);

            if (m_close_func != nullptr)
            {
                m_close_func();
            }

            break;
        }

        /* The window is gone, whoever did it: so is the thread's reason to live. */
        case WM_DESTROY:
        {
            ::PostQuitMessage(0);

            break;
        }

        case QUIT_MESSAGE:
        {
            ::PostQuitMessage(0);

            break;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paint;

            ::BeginPaint(in_window,
                        &paint);
            ::EndPaint  (in_window,
                        &paint);

            render();

            break;
        }

        case WM_SIZE:
        {
            if (in_wparam != SIZE_MINIMIZED)
            {
                resize();
            }

            break;
        }

        default:
        {
            result = ::DefWindowProcW(in_window,
                                      in_message,
                                      in_wparam,
                                      in_lparam);
        }
    }

    return result;
}

void Replay::StereoReplayPlayer::PreviewWindow::on_frame_image(const uint32_t&                            in_n_frame,
                                                               const uint32_t&                            in_n_execution,
                                                               const Replay::StereoReplayEngineDLL::FrameImage& in_image)
{
    (void) in_n_frame;

    if (m_image_buffer.submit(in_n_execution,
                              in_image) )
    {
        if (::PostMessageW(m_hwnd,
                           WM_APP,
                           0,  /* wparam */
                           0) == FALSE) /* lparam */
        {
            m_image_buffer.on_post_failed();
        }
    }
}

template<typename T>
void Replay::StereoReplayPlayer::PreviewWindow::release(T** inout_ptr_ptr)
{
    if (*inout_ptr_ptr != nullptr)
    {
        (*inout_ptr_ptr)->Release();

        *inout_ptr_ptr = nullptr;
    }
}

void Replay::StereoReplayPlayer::PreviewWindow::render()
{
    constexpr float CLEAR_COLOR[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    RECT                                     client_rect  = {};
    std::array<bool, EyeImageBuffer::N_EYES> is_new_array = {};
    RECT                                     left_cell    = {};
    RECT                                     right_cell   = {};

    if (m_render_target_view_ptr != nullptr)
    {
        if (m_image_buffer.take(&m_ui_image_array,
                                &is_new_array) )
        {
            for (uint32_t n_eye = 0;
                          n_eye < EyeImageBuffer::N_EYES;
                        ++n_eye)
            {
                if (is_new_array.at(n_eye) )
                {
                    upload(m_ui_image_array.at(n_eye),
                          &m_eye_texture      [n_eye]);
                }
            }
        }

        ::GetClientRect(m_hwnd,
                       &client_rect);

        left_cell  = client_rect;
        right_cell = client_rect;

        if (m_eye_mode == EyeMode::BOTH)
        {
            left_cell.right = (client_rect.left + client_rect.right) / 2;
            right_cell.left = left_cell.right;
        }

        m_device_context_ptr->OMSetRenderTargets   (1, /* n views */
                                                   &m_render_target_view_ptr,
                                                    nullptr);
        m_device_context_ptr->ClearRenderTargetView(m_render_target_view_ptr,
                                                    CLEAR_COLOR);
        m_device_context_ptr->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_device_context_ptr->VSSetShader          (m_vertex_shader_ptr,
                                                    nullptr,
                                                    0);
        m_device_context_ptr->PSSetShader          (m_pixel_shader_ptr,
                                                    nullptr,
                                                    0);
        m_device_context_ptr->PSSetSamplers        (0, /* start slot */
                                                    1, /* n samplers */
                                                   &m_sampler_ptr);

        if (m_eye_mode == EyeMode::BOTH)
        {
            draw_eye(m_eye_texture[EyeImageBuffer::EYE_LEFT],
                     left_cell);
            draw_eye(m_eye_texture[EyeImageBuffer::EYE_RIGHT],
                     right_cell);
        }
        else
        {
            draw_eye(m_eye_texture[(m_eye_mode == EyeMode::LEFT) ? EyeImageBuffer::EYE_LEFT
                                                                 : EyeImageBuffer::EYE_RIGHT],
                     left_cell);
        }

        m_swap_chain_ptr->Present(0, /* sync interval */
                                  0  /* flags         */);
    }
}

void Replay::StereoReplayPlayer::PreviewWindow::resize()
{
    if (m_swap_chain_ptr != nullptr)
    {
        destroy_render_target();

        m_swap_chain_ptr->ResizeBuffers(0, /* buffer count: kept        */
                                        0, /* width: from the window    */
                                        0, /* height: from the window   */
                                        DXGI_FORMAT_UNKNOWN,
                                        0  /* flags                     */);

        create_render_target();
    }
}

void Replay::StereoReplayPlayer::PreviewWindow::run_thread()
{
    std::string error;
    const bool  is_ok = create_window(&error);

    {
        std::lock_guard<std::mutex> lock(m_startup.mutex);

        m_startup.error   = error;
        m_startup.is_done = true;
        m_startup.is_ok   = is_ok;
    }

    m_startup.cv.notify_all();

    if (is_ok)
    {
        MSG message;

        while (::GetMessageW(&message,
                             nullptr,
                             0,
                             0) > 0)
        {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }

    destroy_resources();
}

void Replay::StereoReplayPlayer::PreviewWindow::set_close_func(const CloseFunc& in_close_func)
{
    m_close_func = in_close_func;
}

void Replay::StereoReplayPlayer::PreviewWindow::set_frame_range(const uint32_t& in_first_frame,
                                                                const uint32_t& in_last_frame)
{
    m_first_frame = in_first_frame;
    m_last_frame  = in_last_frame;
}

void Replay::StereoReplayPlayer::PreviewWindow::upload(const EyeImageBuffer::Image& in_image,
                                                       EyeTexture*                  inout_texture_ptr)
{
    if (in_image.width  != 0 &&
        in_image.height != 0)
    {
        /* An image of another size gets a new texture. */
        if (inout_texture_ptr->tex_ptr != nullptr          &&
            (inout_texture_ptr->height != in_image.height ||
             inout_texture_ptr->width  != in_image.width) )
        {
            release(&inout_texture_ptr->view_ptr);
            release(&inout_texture_ptr->tex_ptr);
        }

        if (inout_texture_ptr->tex_ptr == nullptr)
        {
            D3D11_TEXTURE2D_DESC desc = {};

            desc.ArraySize        = 1;
            desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
            desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.Height           = in_image.height;
            desc.MipLevels        = 1;
            desc.SampleDesc.Count = 1;
            desc.Usage            = D3D11_USAGE_DEFAULT;
            desc.Width            = in_image.width;

            if (SUCCEEDED(m_device_ptr->CreateTexture2D(&desc,
                                                         nullptr,
                                                        &inout_texture_ptr->tex_ptr) ) )
            {
                m_device_ptr->CreateShaderResourceView(inout_texture_ptr->tex_ptr,
                                                       nullptr,
                                                      &inout_texture_ptr->view_ptr);

                inout_texture_ptr->height = in_image.height;
                inout_texture_ptr->width  = in_image.width;
            }
        }

        if (inout_texture_ptr->tex_ptr != nullptr)
        {
            m_device_context_ptr->UpdateSubresource(inout_texture_ptr->tex_ptr,
                                                    0,       /* subresource */
                                                    nullptr, /* box         */
                                                    in_image.pixel_u8_vec.data(),
                                                    in_image.width * sizeof(uint32_t),
                                                    0);      /* depth pitch */
        }
    }
}

bool Replay::StereoReplayPlayer::PreviewWindow::wants_frame_image(const uint32_t& in_n_frame,
                                                                  const uint32_t& in_n_execution) const
{
    /* About the rate of a 60 Hz display: faster than that, the frames in between would never be seen. */
    constexpr std::chrono::milliseconds MIN_INTERVAL(16);

    const auto now = std::chrono::steady_clock::now();

    if (m_image_rate.is_decided      == false      ||
        m_image_rate.n_decided_frame != in_n_frame)
    {
        m_image_rate.is_wanted = (m_is_visible                                          &&
                                  in_n_frame  >= m_first_frame                          &&
                                  (in_n_frame == m_first_frame                          ||
                                   in_n_frame == m_last_frame                           ||
                                   now - m_image_rate.last_wanted_time >= MIN_INTERVAL) );

        if (m_image_rate.is_wanted)
        {
            m_image_rate.last_wanted_time = now;
        }

        m_image_rate.is_decided      = true;
        m_image_rate.n_decided_frame = in_n_frame;
    }

    (void) in_n_execution;

    return m_image_rate.is_wanted;
}

LRESULT CALLBACK Replay::StereoReplayPlayer::PreviewWindow::window_proc(HWND   in_window,
                                                                        UINT   in_message,
                                                                        WPARAM in_wparam,
                                                                        LPARAM in_lparam)
{
    PreviewWindow* window_ptr = nullptr;

    if (in_message == WM_NCCREATE)
    {
        ::SetWindowLongPtrW(in_window,
                            GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(reinterpret_cast<const CREATESTRUCTW*>(in_lparam)->lpCreateParams) );
    }

    window_ptr = reinterpret_cast<PreviewWindow*>(::GetWindowLongPtrW(in_window,
                                                                      GWLP_USERDATA) );

    return (window_ptr != nullptr) ? window_ptr->handle_message(in_window,
                                                                in_message,
                                                                in_wparam,
                                                                in_lparam)
                                   : ::DefWindowProcW          (in_window,
                                                                in_message,
                                                                in_wparam,
                                                                in_lparam);
}

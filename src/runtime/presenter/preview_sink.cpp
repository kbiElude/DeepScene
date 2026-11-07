/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "preview_sink.h"
#include <d3d11.h>
#include <string.h>

Stereo::Presenter::PreviewSink::PreviewSink(ID3D11Device*          in_device_ptr,
                                            ID3D11DeviceContext*   in_context_ptr,
                                            const SizeChangedFunc& in_size_changed_func)
    :m_context_ptr      (in_context_ptr),
     m_device_ptr       (in_device_ptr),
     m_height           (0),
     m_last_frame_index (UINT64_MAX),
     m_size_changed_func(in_size_changed_func),
     m_texture_ptr      (nullptr),
     m_view_ptr         (nullptr),
     m_width            (0)
{
    /* Stub */
}

Stereo::Presenter::PreviewSink::~PreviewSink()
{
    release_texture();
}

uint32_t Stereo::Presenter::PreviewSink::get_height() const
{
    return m_height;
}

ID3D11ShaderResourceView* Stereo::Presenter::PreviewSink::get_view_ptr() const
{
    return m_view_ptr;
}

uint32_t Stereo::Presenter::PreviewSink::get_width() const
{
    return m_width;
}

void Stereo::Presenter::PreviewSink::on_frame(const Stereo::Transport::ImportedFrame& in_frame)
{
    constexpr uint32_t BYTES_PER_PIXEL = 4;

    const Stereo::Transport::FrameInfo& info = in_frame.info;

    /* An image that was shown is not uploaded again. */
    if (info.frame_index != m_last_frame_index)
    {
        if (info.width  != m_width ||
            info.height != m_height)
        {
            D3D11_TEXTURE2D_DESC description = {};

            release_texture();

            description.ArraySize        = 1;
            description.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
            description.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;
            description.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
            description.Height           = info.height;
            description.MipLevels        = 1;
            description.SampleDesc.Count = 1;
            description.Usage            = D3D11_USAGE_DYNAMIC;
            description.Width            = info.width;

            bool          is_created     = false;
            const HRESULT texture_result = m_device_ptr->CreateTexture2D(&description,
                                                                         nullptr,
                                                                        &m_texture_ptr);

            if (SUCCEEDED(texture_result) )
            {
                const HRESULT view_result = m_device_ptr->CreateShaderResourceView(m_texture_ptr,
                                                                                   nullptr,
                                                                                  &m_view_ptr);

                is_created = SUCCEEDED(view_result);
            }

            if (is_created)
            {
                m_height = info.height;
                m_width  = info.width;

                if (m_size_changed_func)
                {
                    m_size_changed_func(m_width,
                                        m_height);
                }
            }
            else
            {
                release_texture();
            }
        }

        if (m_texture_ptr != nullptr)
        {
            D3D11_MAPPED_SUBRESOURCE mapped = {};

            if (SUCCEEDED(m_context_ptr->Map(m_texture_ptr,
                                             0, /* subresource */
                                             D3D11_MAP_WRITE_DISCARD,
                                             0, /* flags */
                                            &mapped) ))
            {
                for (uint32_t n_row = 0;
                              n_row < info.height;
                            ++n_row)
                {
                    ::memcpy(static_cast<uint8_t*>(mapped.pData) + static_cast<size_t>(n_row) * mapped.RowPitch,
                             in_frame.pixel_ptr                  + static_cast<size_t>(n_row) * info.pitch,
                             static_cast<size_t>(info.width) * BYTES_PER_PIXEL);
                }

                m_context_ptr->Unmap(m_texture_ptr,
                                     0);

                m_last_frame_index = info.frame_index;
            }
        }
    }
}

void Stereo::Presenter::PreviewSink::release_texture()
{
    if (m_view_ptr != nullptr)
    {
        m_view_ptr->Release();

        m_view_ptr = nullptr;
    }

    if (m_texture_ptr != nullptr)
    {
        m_texture_ptr->Release();

        m_texture_ptr = nullptr;
    }

    m_height = 0;
    m_width  = 0;
}

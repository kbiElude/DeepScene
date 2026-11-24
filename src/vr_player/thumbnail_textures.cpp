/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "thumbnail_textures.h"
#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>

VRPlayer::ThumbnailTextures::ThumbnailTextures(const size_t& in_n_apps)
    :m_entry_vec(in_n_apps)
{
    /* Nothing to do. */
}

VRPlayer::ThumbnailTextures::~ThumbnailTextures()
{
    for (Entry& entry : m_entry_vec)
    {
        if (entry.view_ptr != nullptr)
        {
            entry.view_ptr->Release();
        }
    }
}

float VRPlayer::ThumbnailTextures::get_aspect(const size_t& in_n_app) const
{
    return m_entry_vec.at(in_n_app).aspect;
}

ID3D11ShaderResourceView* VRPlayer::ThumbnailTextures::get_view_ptr(const size_t& in_n_app) const
{
    return m_entry_vec.at(in_n_app).view_ptr;
}

bool VRPlayer::ThumbnailTextures::is_tried(const size_t& in_n_app) const
{
    return m_entry_vec.at(in_n_app).is_tried;
}

bool VRPlayer::ThumbnailTextures::load(const size_t&      in_n_app,
                                       ID3D11Device*      in_device_ptr,
                                       const std::string& in_file_path)
{
    Entry&                                           entry = m_entry_vec.at(in_n_app);
    Microsoft::WRL::ComPtr<IWICBitmapDecoder>        decoder;
    Microsoft::WRL::ComPtr<IWICImagingFactory>       factory;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode>    frame;
    Microsoft::WRL::ComPtr<IWICFormatConverter>      converter;
    std::vector<uint8_t>                             pixel_vec;
    UINT                                             height = 0;
    UINT                                             width  = 0;
    const std::wstring                               file_path(in_file_path.begin(),
                                                               in_file_path.end() );

    if (entry.is_tried)
    {
        goto end;
    }

    entry.is_tried = true;

    /* The window thread has COM already or gets it here; a thread that has it in another mode is fine for this use. */
    ::CoInitializeEx(nullptr,
                     COINIT_APARTMENTTHREADED);

    if (FAILED(::CoCreateInstance(CLSID_WICImagingFactory,
                                  nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory) )) )
    {
        goto end;
    }

    if (FAILED(factory->CreateDecoderFromFilename(file_path.c_str(),
                                                  nullptr,
                                                  GENERIC_READ,
                                                  WICDecodeMetadataCacheOnDemand,
                                                  &decoder) ))
    {
        goto end;
    }

    if (FAILED(decoder->GetFrame(0,
                                 &frame) ))
    {
        goto end;
    }

    if (FAILED(factory->CreateFormatConverter(&converter) ))
    {
        goto end;
    }

    if (FAILED(converter->Initialize(frame.Get(),
                                     GUID_WICPixelFormat32bppRGBA,
                                     WICBitmapDitherTypeNone,
                                     nullptr,
                                     0.0,
                                     WICBitmapPaletteTypeCustom) ))
    {
        goto end;
    }

    if (FAILED(converter->GetSize(&width,
                                  &height) ) ||
        width  == 0                           ||
        height == 0)
    {
        goto end;
    }

    pixel_vec.resize(static_cast<size_t>(width) * height * 4);

    if (FAILED(converter->CopyPixels(nullptr,
                                     width * 4,
                                     static_cast<UINT>(pixel_vec.size() ),
                                     pixel_vec.data() ) ))
    {
        goto end;
    }

    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        D3D11_SUBRESOURCE_DATA                  data        = {};
        D3D11_TEXTURE2D_DESC                    description = {};

        data.pSysMem     = pixel_vec.data();
        data.SysMemPitch = width * 4;

        description.ArraySize        = 1;
        description.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        description.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.Height           = height;
        description.MipLevels        = 1;
        description.SampleDesc.Count = 1;
        description.Usage            = D3D11_USAGE_IMMUTABLE;
        description.Width            = width;

        if (FAILED(in_device_ptr->CreateTexture2D(&description,
                                                  &data,
                                                  &texture) ))
        {
            goto end;
        }

        if (FAILED(in_device_ptr->CreateShaderResourceView(texture.Get(),
                                                           nullptr,
                                                           &entry.view_ptr) ))
        {
            entry.view_ptr = nullptr;

            goto end;
        }

        entry.aspect = static_cast<float>(width) / static_cast<float>(height);
    }

end:
    return (entry.view_ptr != nullptr);
}

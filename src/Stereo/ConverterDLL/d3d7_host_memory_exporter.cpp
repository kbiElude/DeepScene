/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "../../Stereo/Replay/EngineDLL/pixel_converter.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Hooks/D3D8/types.h"
#include "d3d7_host_memory_exporter.h"
#include <stdio.h>
#include <string.h>

Stereo::ConverterDLL::D3D7HostMemoryExporter::D3D7HostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr)
    :m_channel_ptr   (inout_channel_ptr),
     m_last_frame_qpc(0)
{
    m_channel_ptr->set_offered_kinds(get_offered_kinds() );
}

Stereo::Transport::ExportStatus Stereo::ConverterDLL::D3D7HostMemoryExporter::export_frame(const uint64_t&                        in_frame_index,
                                                                                           const Stereo::Transport::ExportSource& in_source)
{
    std::array<LockedSurface, N_EYES>   locked_array;
    uint32_t                            n_eyes            = (in_source.right_surface_ptr != nullptr) ? 2 : 1;
    uint32_t                            n_locked          = 0;
    uint8_t*                            destination_ptr   = nullptr;
    Stereo::Transport::FrameInfo        info;
    uint32_t                            pitch             = 0;
    Stereo::Transport::ExportStatus     result            = Stereo::Transport::ExportStatus::FAILED;
    std::array<void*, N_EYES>           surface_ptr_array = {in_source.left_surface_ptr, in_source.right_surface_ptr};

    /* The other side has not said that it takes what the exporter passes on: not a failure, the frame is left out. */
    if (m_channel_ptr->get_negotiated_kind() != Stereo::Transport::TransportKind::HOST_MEMORY)
    {
        result = Stereo::Transport::ExportStatus::NOT_READY;

        goto end;
    }

    if (surface_ptr_array.at(0) == nullptr)
    {
        goto end;
    }

    for (uint32_t n_eye = 0;
                  n_eye < n_eyes;
                ++n_eye)
    {
        if (lock_surface(surface_ptr_array.at(n_eye),
                        &locked_array.at(n_eye) ) == false)
        {
            goto end;
        }

        ++n_locked;
    }

    if (n_eyes == 2                                                                         &&
        (locked_array.at(0).description.dwWidth  != locked_array.at(1).description.dwWidth ||
         locked_array.at(0).description.dwHeight != locked_array.at(1).description.dwHeight) )
    {
        goto end;
    }

    info.frame_index       = in_frame_index;
    info.height            = locked_array.at(0).description.dwHeight;
    info.n_eyes            = n_eyes;
    info.timestamp_seconds = in_source.timestamp_seconds;
    info.width             = locked_array.at(0).description.dwWidth * n_eyes;

    destination_ptr = m_channel_ptr->begin_write_ptr(info);

    if (destination_ptr == nullptr)
    {
        goto end;
    }

    pitch = Stereo::Transport::FrameChannel::get_pitch(info.width);

    for (uint32_t n_eye = 0;
                  n_eye < n_eyes;
                ++n_eye)
    {
        if (write_eye(locked_array.at(n_eye),
                      n_eye,
                      destination_ptr,
                      pitch) == false)
        {
            goto end;
        }
    }

    m_channel_ptr->end_write();

    result = Stereo::Transport::ExportStatus::EXPORTED;

end:
    for (uint32_t n_eye = 0;
                  n_eye < n_locked;
                ++n_eye)
    {
        unlock_surface(&locked_array.at(n_eye) );
    }

    return result;
}

Stereo::Transport::TransportKindMask Stereo::ConverterDLL::D3D7HostMemoryExporter::get_offered_kinds() const
{
    return static_cast<Stereo::Transport::TransportKindMask>(Stereo::Transport::TransportKind::HOST_MEMORY);
}

uint32_t Stereo::ConverterDLL::D3D7HostMemoryExporter::get_pixel_format(const Interception::Hooks::D3D7::DDPIXELFORMAT& in_pixel_format)
{
    constexpr uint32_t MASK_BLUE_X1R5G5B5  = 0x0000001f;
    constexpr uint32_t MASK_BLUE_X8R8G8B8  = 0x000000ff;
    constexpr uint32_t MASK_GREEN_R5G6B5   = 0x000007e0;
    constexpr uint32_t MASK_GREEN_X1R5G5B5 = 0x000003e0;
    constexpr uint32_t MASK_GREEN_X8R8G8B8 = 0x0000ff00;
    constexpr uint32_t MASK_RED_R5G6B5     = 0x0000f800;
    constexpr uint32_t MASK_RED_X1R5G5B5   = 0x00007c00;
    constexpr uint32_t MASK_RED_X8R8G8B8   = 0x00ff0000;
    constexpr uint32_t N_BITS_16           = 16;
    constexpr uint32_t N_BITS_32           = 32;

    uint32_t result = 0;

    if ((in_pixel_format.dwFlags & Interception::Core::D3D7Utils::DDPF_RGB) == 0)
    {
        goto end;
    }

    if (in_pixel_format.dwRGBBitCount == N_BITS_32 &&
        in_pixel_format.dwRBitMask    == MASK_RED_X8R8G8B8   &&
        in_pixel_format.dwGBitMask    == MASK_GREEN_X8R8G8B8 &&
        in_pixel_format.dwBBitMask    == MASK_BLUE_X8R8G8B8)
    {
        result = Interception::Hooks::D3D8::D3DFMT_X8R8G8B8;
    }
    else
    if (in_pixel_format.dwRGBBitCount == N_BITS_16 &&
        in_pixel_format.dwRBitMask    == MASK_RED_R5G6B5   &&
        in_pixel_format.dwGBitMask    == MASK_GREEN_R5G6B5 &&
        in_pixel_format.dwBBitMask    == MASK_BLUE_X1R5G5B5)
    {
        result = Interception::Hooks::D3D8::D3DFMT_R5G6B5;
    }
    else
    if (in_pixel_format.dwRGBBitCount == N_BITS_16 &&
        in_pixel_format.dwRBitMask    == MASK_RED_X1R5G5B5   &&
        in_pixel_format.dwGBitMask    == MASK_GREEN_X1R5G5B5 &&
        in_pixel_format.dwBBitMask    == MASK_BLUE_X1R5G5B5)
    {
        result = Interception::Hooks::D3D8::D3DFMT_X1R5G5B5;
    }

end:
    return result;
}

bool Stereo::ConverterDLL::D3D7HostMemoryExporter::lock_surface(void*          in_surface_ptr,
                                                                LockedSurface* out_locked_ptr)
{
    constexpr uint32_t LOCK_READ_ONLY = 0x00000010; /* DDLOCK_READONLY */
    constexpr uint32_t LOCK_WAIT      = 0x00000001; /* DDLOCK_WAIT     */

    void** const vtable = *reinterpret_cast<void***>(in_surface_ptr);

    out_locked_ptr->description        = {};
    out_locked_ptr->description.dwSize = static_cast<DWORD>(sizeof(out_locked_ptr->description) );
    out_locked_ptr->surface_ptr        = static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(in_surface_ptr);

    const auto lock_func = reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_LOCK>(vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_LOCK]);

    out_locked_ptr->is_locked = SUCCEEDED(lock_func(out_locked_ptr->surface_ptr,
                                                    nullptr,
                                                   &out_locked_ptr->description,
                                                    LOCK_READ_ONLY | LOCK_WAIT,
                                                    nullptr) );

    return out_locked_ptr->is_locked;
}

void Stereo::ConverterDLL::D3D7HostMemoryExporter::unlock_surface(LockedSurface* inout_locked_ptr)
{
    if (inout_locked_ptr->is_locked)
    {
        void** const vtable = *reinterpret_cast<void***>(inout_locked_ptr->surface_ptr);

        reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_UNLOCK>(vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_UNLOCK])(inout_locked_ptr->surface_ptr,
                                                                                                                                                                nullptr);

        inout_locked_ptr->is_locked = false;
    }
}

void Stereo::ConverterDLL::D3D7HostMemoryExporter::wait_for_pace()
{
    /* The screen asks about every 14 ms; a wait that is much longer than a few of them is not for it. */
    constexpr uint32_t TIMEOUT_N_MILLISECONDS        = 100;
    constexpr int64_t  FALLBACK_N_FRAMES_PER_SECOND = 60;

    /* With nobody to show the images there is no screen to keep time by, and a window which is not flipped keeps none: the application is held to
     * the rate of a screen, or what it does to the time of its music would not be what it does when it is shown.
     */
    if (m_channel_ptr->get_negotiated_kind() != Stereo::Transport::TransportKind::HOST_MEMORY)
    {
        LARGE_INTEGER frequency;
        LARGE_INTEGER now;

        ::QueryPerformanceFrequency(&frequency);
        ::QueryPerformanceCounter  (&now);

        const LONGLONG frame_n_ticks = frequency.QuadPart / FALLBACK_N_FRAMES_PER_SECOND;
        const LONGLONG due_qpc       = m_last_frame_qpc + frame_n_ticks;

        if (m_last_frame_qpc != 0 &&
            now.QuadPart < due_qpc)
        {
            ::Sleep(static_cast<DWORD>( (due_qpc - now.QuadPart) * 1000 / frequency.QuadPart) );
        }

        ::QueryPerformanceCounter(&now);

        m_last_frame_qpc = now.QuadPart;
    }
    else
    {
        m_channel_ptr->wait_for_pace(TIMEOUT_N_MILLISECONDS);
    }
}

bool Stereo::ConverterDLL::D3D7HostMemoryExporter::write_debug_image(void*              in_left_surface_ptr,
                                                                     void*              in_right_surface_ptr,
                                                                     const std::string& in_path)
{
    constexpr uint32_t BYTES_PER_PIXEL = sizeof(uint32_t);
    constexpr uint32_t HEADER_N_BYTES  = 54;
    constexpr uint32_t N_BITS_PER_PIXEL = 32;
    constexpr uint32_t N_PLANES        = 1;
    constexpr uint32_t INFO_N_BYTES    = 40;

    std::array<LockedSurface, N_EYES> locked_array;
    uint32_t                          n_eyes            = (in_right_surface_ptr != nullptr) ? 2 : 1;
    uint32_t                          n_locked          = 0;
    std::vector<uint8_t>              pixel_u8_vec;
    bool                              result            = false;
    std::array<void*, N_EYES>         surface_ptr_array = {in_left_surface_ptr, in_right_surface_ptr};
    uint32_t                          width             = 0;
    uint32_t                          height            = 0;

    for (uint32_t n_eye = 0;
                  n_eye < n_eyes;
                ++n_eye)
    {
        if (lock_surface(surface_ptr_array.at(n_eye),
                        &locked_array.at(n_eye) ) == false)
        {
            goto end;
        }

        ++n_locked;
    }

    width  = locked_array.at(0).description.dwWidth;
    height = locked_array.at(0).description.dwHeight;

    pixel_u8_vec.assign(static_cast<size_t>(width) * n_eyes * height * BYTES_PER_PIXEL,
                        0);

    for (uint32_t n_eye = 0;
                  n_eye < n_eyes;
                ++n_eye)
    {
        const auto&          description  = locked_array.at(n_eye).description;
        const uint32_t       format       = get_pixel_format(description.ddpfPixelFormat);
        std::vector<uint8_t> converted_u8_vec;
        const uint8_t*       source_ptr   = static_cast<const uint8_t*>(description.lpSurface);
        uint32_t             source_pitch = static_cast<uint32_t>(description.lPitch);

        if (format == 0                                      ||
            description.dwWidth  != width                    ||
            description.dwHeight != height)
        {
            goto end;
        }

        if (format != Interception::Hooks::D3D8::D3DFMT_X8R8G8B8)
        {
            if (Stereo::Replay::EngineDLL::PixelConverter::convert_to_bgra8(format,
                                                                            width,
                                                                            height,
                                                                            source_pitch,
                                                                            source_ptr,
                                                                           &converted_u8_vec) == false)
            {
                goto end;
            }

            source_ptr   = converted_u8_vec.data();
            source_pitch = width * BYTES_PER_PIXEL;
        }

        for (uint32_t n_row = 0;
                      n_row < height;
                    ++n_row)
        {
            ::memcpy(pixel_u8_vec.data() + (static_cast<size_t>(n_row) * width * n_eyes + static_cast<size_t>(n_eye) * width) * BYTES_PER_PIXEL,
                     source_ptr + static_cast<size_t>(n_row) * source_pitch,
                     static_cast<size_t>(width) * BYTES_PER_PIXEL);
        }
    }

    {
        FILE* const file_ptr = ::fopen(in_path.c_str(),
                                       "wb");

        if (file_ptr != nullptr)
        {
            uint8_t        header_u8vec54[HEADER_N_BYTES] = {'B', 'M'};
            const uint32_t file_n_bytes                    = HEADER_N_BYTES + static_cast<uint32_t>(pixel_u8_vec.size() );
            const uint32_t pixel_offset                    = HEADER_N_BYTES;
            const uint32_t total_width                     = width * n_eyes;
            const int32_t  top_down_height                 = -static_cast<int32_t>(height); /* the top row first */
            const uint16_t n_planes                        = N_PLANES;
            const uint16_t n_bits                          = N_BITS_PER_PIXEL;

            ::memcpy(header_u8vec54 + 2,  &file_n_bytes,     sizeof(file_n_bytes) );
            ::memcpy(header_u8vec54 + 10, &pixel_offset,     sizeof(pixel_offset) );
            ::memcpy(header_u8vec54 + 14, &INFO_N_BYTES,     sizeof(INFO_N_BYTES) );
            ::memcpy(header_u8vec54 + 18, &total_width,      sizeof(total_width) );
            ::memcpy(header_u8vec54 + 22, &top_down_height,  sizeof(top_down_height) );
            ::memcpy(header_u8vec54 + 26, &n_planes,         sizeof(n_planes) );
            ::memcpy(header_u8vec54 + 28, &n_bits,           sizeof(n_bits) );

            result = (::fwrite(header_u8vec54, 1, sizeof(header_u8vec54), file_ptr) == sizeof(header_u8vec54) &&
                      ::fwrite(pixel_u8_vec.data(), 1, pixel_u8_vec.size(), file_ptr) == pixel_u8_vec.size() );

            ::fclose(file_ptr);
        }
    }

end:
    for (uint32_t n_eye = 0;
                  n_eye < n_locked;
                ++n_eye)
    {
        unlock_surface(&locked_array.at(n_eye) );
    }

    return result;
}

bool Stereo::ConverterDLL::D3D7HostMemoryExporter::write_eye(const LockedSurface& in_locked,
                                                             const uint32_t&      in_eye,
                                                             uint8_t*             out_destination_ptr,
                                                             const uint32_t&      in_pitch)
{
    constexpr uint32_t BYTES_PER_PIXEL = sizeof(uint32_t);

    const auto&    description  = in_locked.description;
    const uint32_t format       = get_pixel_format(description.ddpfPixelFormat);
    bool           is_ok        = (format != 0                                  &&
                                   description.lpSurface != nullptr);
    const uint8_t* source_ptr   = static_cast<const uint8_t*>(description.lpSurface);
    uint32_t       source_pitch = static_cast<uint32_t>(description.lPitch);

    /* The formats of 32 bits are as they are wanted; the others are converted first. */
    if (is_ok                                                      &&
        format != Interception::Hooks::D3D8::D3DFMT_X8R8G8B8)
    {
        is_ok = Stereo::Replay::EngineDLL::PixelConverter::convert_to_bgra8(format,
                                                                            description.dwWidth,
                                                                            description.dwHeight,
                                                                            source_pitch,
                                                                            source_ptr,
                                                                           &m_converted_u8_vec_array.at(in_eye) );

        source_ptr   = m_converted_u8_vec_array.at(in_eye).data();
        source_pitch = description.dwWidth * BYTES_PER_PIXEL;
    }

    for (uint32_t n_row = 0;
                  n_row < description.dwHeight && is_ok;
                ++n_row)
    {
        ::memcpy(out_destination_ptr + static_cast<size_t>(n_row) * in_pitch + static_cast<size_t>(in_eye) * description.dwWidth * BYTES_PER_PIXEL,
                 source_ptr          + static_cast<size_t>(n_row) * source_pitch,
                 static_cast<size_t>(description.dwWidth) * BYTES_PER_PIXEL);
    }

    return is_ok;
}

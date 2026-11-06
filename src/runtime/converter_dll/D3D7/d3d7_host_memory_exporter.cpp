/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "common/vtable.h"
#include "interception/core/D3D7Utils/constants.h"
#include "interception/hooks/D3D8/types.h"
#include "d3d7_host_memory_exporter.h"
#include "environment.h"
#include "pixel_row_writer.h"
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

Stereo::ConverterDLL::D3D7HostMemoryExporter::D3D7HostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr)
    :HostMemoryExporter(inout_channel_ptr),
     m_last_frame_qpc(0)
{
    /* A wait of the length of a frame is not the length of a tick of the timer, which is some 15 ms: without this the application is held to half
     * the rate it is meant to have, when there is no program that shows the images to keep time by.
     */
    ::timeBeginPeriod(1);
}

Stereo::ConverterDLL::D3D7HostMemoryExporter::~D3D7HostMemoryExporter()
{
    ::timeEndPeriod(1);
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
    if (is_host_memory_negotiated() == false)
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

    destination_ptr = get_channel_ptr()->begin_write_ptr(info);

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

    get_channel_ptr()->end_write();

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

    void** const vtable = APIInterceptor::get_vtable_ptr(in_surface_ptr);

    out_locked_ptr->description        = {};
    out_locked_ptr->description.dwSize = static_cast<DWORD>(sizeof(out_locked_ptr->description) );
    out_locked_ptr->surface_ptr        = static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(in_surface_ptr);

    const auto lock_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_LOCK>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_LOCK);

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
        void** const vtable = APIInterceptor::get_vtable_ptr(inout_locked_ptr->surface_ptr);

        APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_UNLOCK>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_UNLOCK)(inout_locked_ptr->surface_ptr,
                                                                                                                                                                nullptr);

        inout_locked_ptr->is_locked = false;
    }
}

void Stereo::ConverterDLL::D3D7HostMemoryExporter::wait_for_pace()
{
    constexpr int64_t FALLBACK_N_FRAMES_PER_SECOND = 60;

    /* A screen that keeps time says when the next frame is due. With nobody to show the images, or one that does not say (a window shows them as they
     * come), there is no screen to keep time by, and a window which is not flipped keeps none: the application is held to the rate of a screen, or
     * what it does to the time of its music would not be what it does when it is shown.
     */
    const bool is_paced_by_screen = (is_host_memory_negotiated() &&
                                     wait_for_screen() );

    if (is_paced_by_screen == false &&
        Environment::is_set("APISTEREO_NO_FRAME_LIMIT") == false) /* a debugging aid: the cost of a frame, with nothing to wait for */
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
    if (is_paced_by_screen)
    {
        /* The next frame of the limiter is counted from this one if the screen stops keeping time. */
        LARGE_INTEGER now;

        ::QueryPerformanceCounter(&now);

        m_last_frame_qpc = now.QuadPart;
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
    std::vector<uint8_t>              scratch_u8_vec;
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
        const auto&    description = locked_array.at(n_eye).description;
        const uint32_t format      = get_pixel_format(description.ddpfPixelFormat);

        if (format == 0                                      ||
            description.dwWidth  != width                    ||
            description.dwHeight != height                   ||
            PixelRowWriter::write_eye(format,
                                      width,
                                      height,
                                      static_cast<const uint8_t*>(description.lpSurface),
                                      static_cast<uint32_t>(description.lPitch),
                                      n_eye,
                                      pixel_u8_vec.data(),
                                      width * n_eyes * BYTES_PER_PIXEL,
                                     &scratch_u8_vec) == false)
        {
            goto end;
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
    const auto&    description = in_locked.description;
    const uint32_t format      = get_pixel_format(description.ddpfPixelFormat);

    return (format != 0                         &&
            description.lpSurface != nullptr    &&
            PixelRowWriter::write_eye(format,
                                      description.dwWidth,
                                      description.dwHeight,
                                      static_cast<const uint8_t*>(description.lpSurface),
                                      static_cast<uint32_t>(description.lPitch),
                                      in_eye,
                                      out_destination_ptr,
                                      in_pitch,
                                     &m_converted_u8_vec_array.at(in_eye) ) );
}

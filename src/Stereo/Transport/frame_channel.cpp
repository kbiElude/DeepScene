/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/Transport/frame_channel.h"
#include <string.h>

/* What the processes share: the header, then the images. The fields are in the order of the memory (this is a format which two
 * programs agree on), and the 64-bit one is first so that it is aligned. Zeroed memory is a channel with nothing in it, which is
 * how it starts, so no side has to make it ready before the other uses it.
 */
struct Stereo::Transport::FrameChannel::Header
{
    struct Slot
    {
        uint64_t frame_index;
        uint32_t height;
        uint32_t kind;         /* a TransportKind         */
        uint32_t n_eyes;
        uint32_t pitch;
        uint32_t pixel_format; /* a PixelFormat           */
        double   timestamp_seconds;
        uint32_t width;
    };

    volatile LONG64 n_frames_published;
    volatile LONG64 pace_signal_qpc;           /* when the consumer signalled last, on the clock of the system; 0: never */
    volatile LONG   consumer_accepted_kinds;   /* a mask of TransportKind; 0: not said yet               */
    volatile LONG   convergence_distance_bits; /* of a float; valid if has_convergence_distance is not 0 */
    volatile LONG   depth_strength_bits;       /* of a float; valid if has_depth_strength is not 0       */
    volatile LONG   eye_separation_bits;       /* of a float; valid if has_eye_separation is not 0       */
    volatile LONG   has_convergence_distance;  /* 0: none set                                            */
    volatile LONG   has_depth_strength;        /* 0: none set                                            */
    volatile LONG   has_eye_separation;        /* 0: none set                                            */
    volatile LONG   has_ipd_scale;             /* 0: none set                                            */
    volatile LONG   has_target_eye_size;       /* 0: none set                                            */
    volatile LONG   ipd_scale_bits;            /* of a float; valid if has_ipd_scale is not 0            */
    volatile LONG   latest_slot_plus_one;      /* 0: none yet                                            */
    volatile LONG   magic;
    volatile LONG   pace_delay_us;             /* how long the producer waits after a signal              */
    volatile LONG   overlay_is_pushed;         /* 0: not; the consumer says that the person wants it       */
    volatile LONG   producer_offered_kinds;    /* a mask of TransportKind; 0: not said yet               */
    volatile LONG   reading_slot_plus_one;     /* 0: none                                                */
    volatile LONG   target_eye_height;         /* valid if has_target_eye_size is not 0                  */
    volatile LONG   target_eye_width;          /* valid if has_target_eye_size is not 0                  */
    volatile LONG   version;
    Slot            slot_array[3];
};

constexpr uint32_t BYTES_PER_PIXEL = 4;
constexpr uint32_t HEADER_N_BYTES  = 65536;      /* the images start where a GPU places a heap's buffers: at a multiple of 64 KB */
constexpr LONG     MAGIC           = 0x52545341; /* "ASTR" */
constexpr uint32_t MAX_HEIGHT      = 4096;       /* of a texture that Direct3D 8 hardware makes at most */
constexpr uint32_t MAX_WIDTH       = 8192;       /* both eyes of 4096                                   */
constexpr uint32_t N_SLOTS         = 3;
constexpr uint32_t PITCH_ALIGNMENT = 256;        /* of a row of a buffer that a GPU copies to a texture */
constexpr LONG     VERSION         = 12;

/* A multiple of 64 KB: the pitch of the widest image is a multiple of 256 */
constexpr uint64_t SLOT_N_BYTES    = static_cast<uint64_t>(MAX_WIDTH) * BYTES_PER_PIXEL * MAX_HEIGHT;

static_assert(SLOT_N_BYTES % 65536 == 0,
              "A slot has to start where a GPU places a buffer");


Stereo::Transport::FrameChannel::FrameChannel(const HANDLE& in_mapping,
                                              const HANDLE& in_pace_event,
                                              Header*       in_header_ptr)
    :m_header_ptr  (in_header_ptr),
     m_mapping     (in_mapping),
     m_n_write_slot(0),
     m_pace_event  (in_pace_event),
     m_pace_timer  (nullptr)
{
    /* Stub */
}

Stereo::Transport::FrameChannel::~FrameChannel()
{
    ::UnmapViewOfFile(m_header_ptr);

    if (m_pace_timer != nullptr)
    {
        ::CloseHandle(m_pace_timer);
    }

    ::CloseHandle    (m_pace_event);
    ::CloseHandle    (m_mapping);
}

uint8_t* Stereo::Transport::FrameChannel::begin_write_ptr(const FrameInfo& in_info)
{
    const LONG     latest_slot  = m_header_ptr->latest_slot_plus_one  - 1;
    const uint32_t pitch        = get_pitch(in_info.width);
    const LONG     reading_slot = m_header_ptr->reading_slot_plus_one - 1;
    uint8_t*       result_ptr   = nullptr;

    if (in_info.width                   > 0           &&
        in_info.width                   <= MAX_WIDTH  &&
        in_info.height                  > 0           &&
        in_info.height                  <= MAX_HEIGHT &&
        in_info.n_eyes                  >= 1          &&
        in_info.n_eyes                  <= 2          &&
        in_info.width  % in_info.n_eyes == 0          &&
        static_cast<uint64_t>(pitch) * in_info.height <= SLOT_N_BYTES)
    {
        /* Of three slots, one is the newest and one may be read: the third is free. */
        for (uint32_t n_slot = 0;
                      n_slot < N_SLOTS;
                    ++n_slot)
        {
            if (result_ptr                == nullptr       &&
                static_cast<LONG>(n_slot) != latest_slot   &&
                static_cast<LONG>(n_slot) != reading_slot)
            {
                m_n_write_slot = n_slot;
                result_ptr     = get_slot_pixel_ptr(n_slot);
            }
        }

        if (result_ptr != nullptr)
        {
            /* What is passed is in the memory of the channel, whatever the producer says. */
            m_header_ptr->slot_array[m_n_write_slot].frame_index  = in_info.frame_index;
            m_header_ptr->slot_array[m_n_write_slot].height       = in_info.height;
            m_header_ptr->slot_array[m_n_write_slot].kind         = static_cast<uint32_t>(TransportKind::HOST_MEMORY);
            m_header_ptr->slot_array[m_n_write_slot].n_eyes       = in_info.n_eyes;
            m_header_ptr->slot_array[m_n_write_slot].pitch        = pitch;
            m_header_ptr->slot_array[m_n_write_slot].pixel_format = static_cast<uint32_t>(PixelFormat::B8G8R8A8);
            m_header_ptr->slot_array[m_n_write_slot].timestamp_seconds = in_info.timestamp_seconds;
            m_header_ptr->slot_array[m_n_write_slot].width        = in_info.width;
        }
    }

    return result_ptr;
}

bool Stereo::Transport::FrameChannel::begin_read(FrameInfo*      out_info_ptr,
                                                 const uint8_t** out_pixel_ptr_ptr)
{
    constexpr uint32_t N_TRIES = 8;

    bool result = false;

    /* The slot which was the newest may have been left behind by a new image before it is marked as read: then the newest is
     * looked for again.
     */
    for (uint32_t n_try = 0;
                  n_try < N_TRIES && result == false;
                ++n_try)
    {
        const LONG latest_plus_one = m_header_ptr->latest_slot_plus_one;

        if (latest_plus_one == 0)
        {
            break;
        }

        ::InterlockedExchange(&m_header_ptr->reading_slot_plus_one,
                              latest_plus_one);

        if (m_header_ptr->latest_slot_plus_one == latest_plus_one)
        {
            const auto& slot = m_header_ptr->slot_array[latest_plus_one - 1];

            out_info_ptr->frame_index  = slot.frame_index;
            out_info_ptr->height       = slot.height;
            out_info_ptr->kind         = static_cast<TransportKind>(slot.kind);
            out_info_ptr->n_eyes       = slot.n_eyes;
            out_info_ptr->pitch        = slot.pitch;
            out_info_ptr->pixel_format = static_cast<PixelFormat>(slot.pixel_format);
            out_info_ptr->timestamp_seconds = slot.timestamp_seconds;
            out_info_ptr->width        = slot.width;
            *out_pixel_ptr_ptr         = get_slot_pixel_ptr(static_cast<uint32_t>(latest_plus_one - 1) );
            result                     = true;
        }
    }

    if (result == false)
    {
        end_read();
    }

    return result;
}

void Stereo::Transport::FrameChannel::end_read()
{
    ::InterlockedExchange(&m_header_ptr->reading_slot_plus_one,
                          0);
}

void Stereo::Transport::FrameChannel::end_write()
{
    ::InterlockedExchange   (&m_header_ptr->latest_slot_plus_one,
                             static_cast<LONG>(m_n_write_slot) + 1);
    ::InterlockedIncrement64(&m_header_ptr->n_frames_published);
}

Stereo::Transport::TransportKindMask Stereo::Transport::FrameChannel::get_accepted_kinds() const
{
    return static_cast<TransportKindMask>(m_header_ptr->consumer_accepted_kinds);
}

bool Stereo::Transport::FrameChannel::get_convergence_distance(float* out_convergence_distance_ptr) const
{
    const bool result = (m_header_ptr->has_convergence_distance != 0);

    if (result)
    {
        const LONG bits = m_header_ptr->convergence_distance_bits;

        ::memcpy(out_convergence_distance_ptr,
                 &bits,
                 sizeof(float) );
    }

    return result;
}

bool Stereo::Transport::FrameChannel::get_depth_strength(float* out_depth_strength_ptr) const
{
    const bool result = (m_header_ptr->has_depth_strength != 0);

    if (result)
    {
        const LONG bits = m_header_ptr->depth_strength_bits;

        ::memcpy(out_depth_strength_ptr,
                 &bits,
                 sizeof(float) );
    }

    return result;
}

bool Stereo::Transport::FrameChannel::get_eye_separation(float* out_eye_separation_ptr) const
{
    const bool result = (m_header_ptr->has_eye_separation != 0);

    if (result)
    {
        const LONG bits = m_header_ptr->eye_separation_bits;

        ::memcpy(out_eye_separation_ptr,
                 &bits,
                 sizeof(float) );
    }

    return result;
}

uint32_t Stereo::Transport::FrameChannel::get_max_height()
{
    return MAX_HEIGHT;
}

uint32_t Stereo::Transport::FrameChannel::get_max_width()
{
    return MAX_WIDTH;
}

bool Stereo::Transport::FrameChannel::get_ipd_scale(float* out_ipd_scale_ptr) const
{
    const bool result = (m_header_ptr->has_ipd_scale != 0);

    if (result)
    {
        const LONG bits = m_header_ptr->ipd_scale_bits;

        ::memcpy(out_ipd_scale_ptr,
                 &bits,
                 sizeof(float) );
    }

    return result;
}

bool Stereo::Transport::FrameChannel::get_is_overlay_pushed() const
{
    return (m_header_ptr->overlay_is_pushed != 0);
}

uint64_t Stereo::Transport::FrameChannel::get_n_frames_published() const
{
    return static_cast<uint64_t>(::InterlockedCompareExchange64(&m_header_ptr->n_frames_published,
                                                                0,
                                                                0) );
}

Stereo::Transport::TransportKind Stereo::Transport::FrameChannel::get_negotiated_kind() const
{
    const TransportKindMask common = get_accepted_kinds() & get_offered_kinds();
    TransportKind           result = TransportKind::NONE;

    /* The kinds are in the order of how much they save: the highest bit that both have is the best. */
    for (uint32_t n_bit = 0;
                  n_bit < 32;
                ++n_bit)
    {
        if ((common & (1u << n_bit)) != 0)
        {
            result = static_cast<TransportKind>(1u << n_bit);
        }
    }

    return result;
}

bool Stereo::Transport::FrameChannel::get_target_eye_size(uint32_t* out_width_ptr,
                                                          uint32_t* out_height_ptr) const
{
    const bool result = (m_header_ptr->has_target_eye_size != 0);

    if (result)
    {
        *out_height_ptr = static_cast<uint32_t>(m_header_ptr->target_eye_height);
        *out_width_ptr  = static_cast<uint32_t>(m_header_ptr->target_eye_width);
    }

    return result;
}

Stereo::Transport::TransportKindMask Stereo::Transport::FrameChannel::get_offered_kinds() const
{
    return static_cast<TransportKindMask>(m_header_ptr->producer_offered_kinds);
}

uint32_t Stereo::Transport::FrameChannel::get_pitch(const uint32_t& in_width)
{
    return (in_width * BYTES_PER_PIXEL + PITCH_ALIGNMENT - 1) / PITCH_ALIGNMENT * PITCH_ALIGNMENT;
}

uint8_t* Stereo::Transport::FrameChannel::get_slot_pixel_ptr(const uint32_t& in_n_slot) const
{
    return reinterpret_cast<uint8_t*>(m_header_ptr) + HEADER_N_BYTES + in_n_slot * SLOT_N_BYTES;
}

std::unique_ptr<Stereo::Transport::FrameChannel> Stereo::Transport::FrameChannel::open_ptr(const std::string& in_session_id,
                                                                                           std::string*       out_error_ptr)
{
    constexpr uint64_t N_BYTES = HEADER_N_BYTES + N_SLOTS * SLOT_N_BYTES;

    Header*                        header_ptr = nullptr;
    HANDLE                         mapping    = nullptr;
    HANDLE                         pace_event = nullptr;
    std::unique_ptr<FrameChannel>  result_ptr;

    mapping = ::CreateFileMappingA(INVALID_HANDLE_VALUE,
                                   nullptr,
                                   PAGE_READWRITE,
                                   static_cast<DWORD>(N_BYTES >> 32),
                                   static_cast<DWORD>(N_BYTES & 0xFFFFFFFFu),
                                   ("Local\\APIStereo_" + in_session_id).c_str() );

    if (mapping == nullptr)
    {
        *out_error_ptr = "The memory for the images cannot be shared.";

        goto end;
    }

    header_ptr = static_cast<Header*>(::MapViewOfFile(mapping,
                                                      FILE_MAP_ALL_ACCESS,
                                                      0,  /* offset, high */
                                                      0,  /* offset, low  */
                                                      0)); /* all of it   */

    if (header_ptr == nullptr)
    {
        *out_error_ptr = "The memory for the images cannot be mapped.";

        goto end;
    }

    /* The first side to get here stamps the memory; the other finds the stamp. */
    ::InterlockedCompareExchange(&header_ptr->magic,
                                 MAGIC,
                                 0);
    ::InterlockedCompareExchange(&header_ptr->version,
                                 VERSION,
                                 0);

    if (header_ptr->magic   != MAGIC ||
        header_ptr->version != VERSION)
    {
        *out_error_ptr = "The memory for the images is of another version.";

        ::UnmapViewOfFile(header_ptr);

        goto end;
    }

    /* An event that a signal of the consumer sets, and the producer's wait takes: it holds one signal at most. */
    pace_event = ::CreateEventA(nullptr,
                                FALSE, /* bManualReset */
                                FALSE, /* bInitialState */
                                ("Local\\APIStereo_" + in_session_id + "_pace").c_str() );

    if (pace_event == nullptr)
    {
        *out_error_ptr = "The event for the pace of the frames cannot be made.";

        ::UnmapViewOfFile(header_ptr);

        goto end;
    }

    result_ptr.reset(new FrameChannel(mapping,
                                      pace_event,
                                      header_ptr) );
    mapping = nullptr;

end:
    if (mapping != nullptr)
    {
        ::CloseHandle(mapping);
    }

    return result_ptr;
}

void Stereo::Transport::FrameChannel::set_accepted_kinds(const TransportKindMask& in_kinds)
{
    ::InterlockedExchange(&m_header_ptr->consumer_accepted_kinds,
                          static_cast<LONG>(in_kinds) );
}

void Stereo::Transport::FrameChannel::set_convergence_distance(const float& in_convergence_distance)
{
    LONG bits = 0;

    ::memcpy(&bits,
             &in_convergence_distance,
             sizeof(float) );

    /* The value first, then the flag that says that there is one. */
    ::InterlockedExchange(&m_header_ptr->convergence_distance_bits,
                          bits);
    ::InterlockedExchange(&m_header_ptr->has_convergence_distance,
                          1);
}

void Stereo::Transport::FrameChannel::set_depth_strength(const float& in_depth_strength)
{
    LONG bits = 0;

    ::memcpy(&bits,
             &in_depth_strength,
             sizeof(float) );

    /* The value first, then the flag that says that there is one. */
    ::InterlockedExchange(&m_header_ptr->depth_strength_bits,
                          bits);
    ::InterlockedExchange(&m_header_ptr->has_depth_strength,
                          1);
}

void Stereo::Transport::FrameChannel::set_eye_separation(const float& in_eye_separation)
{
    LONG bits = 0;

    ::memcpy(&bits,
             &in_eye_separation,
             sizeof(float) );

    /* The value first, then the flag that says that there is one. */
    ::InterlockedExchange(&m_header_ptr->eye_separation_bits,
                          bits);
    ::InterlockedExchange(&m_header_ptr->has_eye_separation,
                          1);
}

void Stereo::Transport::FrameChannel::set_pace_delay_us(const uint32_t& in_delay_us)
{
    ::InterlockedExchange(&m_header_ptr->pace_delay_us,
                          static_cast<LONG>(in_delay_us) );
}

void Stereo::Transport::FrameChannel::set_target_eye_size(const uint32_t& in_width,
                                                          const uint32_t& in_height)
{
    /* The values first, then the flag that says that there are some. */
    ::InterlockedExchange(&m_header_ptr->target_eye_height,
                          static_cast<LONG>(in_height) );
    ::InterlockedExchange(&m_header_ptr->target_eye_width,
                          static_cast<LONG>(in_width) );
    ::InterlockedExchange(&m_header_ptr->has_target_eye_size,
                          1);
}

void Stereo::Transport::FrameChannel::set_ipd_scale(const float& in_ipd_scale)
{
    LONG bits = 0;

    ::memcpy(&bits,
             &in_ipd_scale,
             sizeof(float) );

    /* The value first, then the flag that says that there is one. */
    ::InterlockedExchange(&m_header_ptr->ipd_scale_bits,
                          bits);
    ::InterlockedExchange(&m_header_ptr->has_ipd_scale,
                          1);
}

void Stereo::Transport::FrameChannel::set_is_overlay_pushed(const bool& in_is_pushed)
{
    ::InterlockedExchange(&m_header_ptr->overlay_is_pushed,
                          (in_is_pushed) ? 1
                                         : 0);
}

void Stereo::Transport::FrameChannel::set_offered_kinds(const TransportKindMask& in_kinds)
{
    ::InterlockedExchange(&m_header_ptr->producer_offered_kinds,
                          static_cast<LONG>(in_kinds) );
}

void Stereo::Transport::FrameChannel::signal_pace()
{
    LARGE_INTEGER now;

    ::QueryPerformanceCounter(&now);

    ::InterlockedExchange64(&m_header_ptr->pace_signal_qpc,
                            now.QuadPart);
    ::SetEvent             (m_pace_event);
}

bool Stereo::Transport::FrameChannel::wait_for_pace(const uint32_t& in_timeout_n_milliseconds)
{
    /* A consumer that has not signalled for this long is not there: the producer is not to wait for what does not come. */
    constexpr LONGLONG ALIVE_N_MILLISECONDS = 250;

    LARGE_INTEGER frequency;
    LARGE_INTEGER now;
    bool          result = false;

    ::QueryPerformanceCounter  (&now);
    ::QueryPerformanceFrequency(&frequency);

    const LONGLONG signalled_qpc = m_header_ptr->pace_signal_qpc;

    if (signalled_qpc != 0                                                                 &&
        (now.QuadPart - signalled_qpc) * 1000 <= ALIVE_N_MILLISECONDS * frequency.QuadPart)
    {
        /* A signal that came while the producer was busy is there already; if not, the next one is waited for. */
        bool is_signalled = (::WaitForSingleObject(m_pace_event,
                                                   0) == WAIT_OBJECT_0);

        if (is_signalled == false)
        {
            is_signalled = (::WaitForSingleObject(m_pace_event,
                                                  in_timeout_n_milliseconds) == WAIT_OBJECT_0);

            result = is_signalled;
        }

        if (is_signalled)
        {
            /* The moment is counted from the last signal, which may be a later one than the one that was taken. */
            const LONGLONG delay_qpc = static_cast<LONGLONG>(static_cast<uint32_t>(m_header_ptr->pace_delay_us) ) * frequency.QuadPart / 1000000;
            const LONGLONG target    = static_cast<LONGLONG>(m_header_ptr->pace_signal_qpc) + delay_qpc;

            ::QueryPerformanceCounter(&now);

            if (now.QuadPart < target)
            {
                wait_until(target,
                           frequency.QuadPart);

                result = true;
            }
        }
    }

    return result;
}

void Stereo::Transport::FrameChannel::wait_until(const LONGLONG& in_target_qpc,
                                                 const LONGLONG& in_frequency)
{
    /* A timer that wakes it about a millisecond early, if the system has one that is fine enough, and the rest is looked at: a wait that
     * is as coarse as a tick of the system would be a frame.
     */
    constexpr LONGLONG EARLY_N_MICROSECONDS = 1000;

    LARGE_INTEGER now;

    ::QueryPerformanceCounter(&now);

    if (m_pace_timer == nullptr)
    {
        m_pace_timer = ::CreateWaitableTimerExW(nullptr,
                                                nullptr,
                                                CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                                TIMER_ALL_ACCESS);
    }

    const LONGLONG remaining_us = (in_target_qpc - now.QuadPart) * 1000000 / in_frequency;

    if (m_pace_timer != nullptr &&
        remaining_us > EARLY_N_MICROSECONDS)
    {
        LARGE_INTEGER due;

        /* Relative, in units of 100 ns, and negative for that. */
        due.QuadPart = -(remaining_us - EARLY_N_MICROSECONDS) * 10;

        if (::SetWaitableTimer(m_pace_timer,
                               &due,
                               0,
                               nullptr,
                               nullptr,
                               FALSE) != FALSE)
        {
            ::WaitForSingleObject(m_pace_timer,
                                  INFINITE);
        }
    }

    do
    {
        ::SwitchToThread();

        ::QueryPerformanceCounter(&now);
    }
    while (now.QuadPart < in_target_qpc);
}

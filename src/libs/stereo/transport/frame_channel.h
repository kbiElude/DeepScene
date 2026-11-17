/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Passes images between processes (also 32-bit to 64-bit) through shared memory with three image slots. Neither side waits for the
 * other: the producer writes a slot that is neither the newest nor being read, the consumer reads the newest.
 *
 * The first side to open creates the channel. One side writes, the other reads (not checked).
 * Slots start at multiples of 64 KB and rows at multiples of 256 bytes (see get_pitch()), so the GPU can import it as a heap.
 * The sides negotiate the transport kind (set_offered_kinds(), set_accepted_kinds()); the best common one is used.
 */
#ifndef STEREO_TRANSPORT_FRAME_CHANNEL_H
#define STEREO_TRANSPORT_FRAME_CHANNEL_H

#include "stereo/transport/types.h"
#include <memory>
#include <string>
#include <Windows.h>

namespace Stereo
{
    namespace Transport
    {
        class FrameChannel
        {
        public:
            ~FrameChannel();

            /* Producer. Returns the memory to write to (rows one pitch apart), or nullptr if the image does not fit. Hidden until end_write(). */
            uint8_t* begin_write_ptr(const FrameInfo& in_info);

            /* Consumer. Returns the newest image, unchanged until end_read(); false if there is none yet. */
            bool begin_read(FrameInfo*      out_info_ptr,
                            const uint8_t** out_pixel_ptr_ptr);

            void end_read();

            /* Makes the image of begin_write_ptr() the newest. */
            void end_write();

            /* Kinds set by the consumer; none until set. */
            TransportKindMask get_accepted_kinds() const;

            /* Convergence distance in the application's matrix units, set by the consumer at any time. False if unset (the producer keeps its own). */
            bool get_convergence_distance(float* out_convergence_distance_ptr) const;

            /* 3D strength: 1 natural, 0 none, 2 twice natural. Consumer-set at any time; false if unset (the producer keeps its eye separation). */
            bool get_depth_strength(float* out_depth_strength_ptr) const;

            /* Eye separation for the producer, consumer-set at any time. False if unset (the producer keeps its own). */
            bool get_eye_separation(float* out_eye_separation_ptr) const;

            /* True once the consumer has set whether pushed draws are pushed (see get_is_overlay_pushed()); until then the producer
             * follows Comfort::pushes_by_default.
             */
            bool get_has_overlay_push_setting() const;

            /* True if draws the recipe leaves alone are pushed toward the viewer (distance: Comfort::overlay_parallax). Consumer-set at any time; false until set. */
            bool get_is_overlay_pushed() const;

            /* Viewer eye distance over the eye distance the separation is made for (1: equal). Consumer-set at any time; false if unset (taken as 1). */
            bool get_ipd_scale(float* out_ipd_scale_ptr) const;

            /* Images made the newest since the channel was created. */
            uint64_t get_n_frames_published() const;

            /* Highest bit common to both masks; NONE until both are set. */
            TransportKind get_negotiated_kind() const;

            /* Kinds set by the producer; none until set. */
            TransportKindMask get_offered_kinds() const;

            /* Largest per-eye image size to render, consumer-set (the headset's size). False if unset: the consumer shows no images and the
             * application stays on screen. 0 x 0: the consumer shows the images and the producer renders at the application's size.
             * Otherwise the application's output is enlarged to the largest size that fits, keeping proportions. Read once when the
             * application creates its device, so set it before the application starts.
             */
            bool get_target_eye_size(uint32_t* out_width_ptr,
                                     uint32_t* out_height_ptr) const;

            /* Consumer. Sets the kinds of transport it can take. */
            void set_accepted_kinds(const TransportKindMask& in_kinds);

            /* See get_convergence_distance(). */
            void set_convergence_distance(const float& in_convergence_distance);

            /* See get_depth_strength(). */
            void set_depth_strength(const float& in_depth_strength);

            /* See get_eye_separation(). */
            void set_eye_separation(const float& in_eye_separation);

            /* See get_ipd_scale(). */
            void set_ipd_scale(const float& in_ipd_scale);

            /* See get_is_overlay_pushed(). */
            void set_is_overlay_pushed(const bool& in_is_pushed);

            /* Producer. Sets the kinds of transport it can pass images by. */
            void set_offered_kinds(const TransportKindMask& in_kinds);

            /* Consumer. Delay in microseconds between signal_pace() and the producer going on. */
            void set_pace_delay_us(const uint32_t& in_delay_us);

            /* See get_target_eye_size(). */
            void set_target_eye_size(const uint32_t& in_width,
                                     const uint32_t& in_height);

            /* Consumer. The screen is about to want a frame; releases a producer in wait_for_pace() (one frame per screen frame). */
            void signal_pace();

            /* Producer. Waits for a new signal if none came since the last call, then until the delay has passed, counted from the
             * signal. Returns true if it had to wait; false if it did not (no signals, producer late, or no signal within
             * in_timeout_n_milliseconds). The producer goes on either way.
             */
            bool wait_for_pace(const uint32_t& in_timeout_n_milliseconds);


            /* Returns nullptr and sets out_error_ptr if the channel cannot be opened. Processes with the same session ID share it. */
            static std::unique_ptr<FrameChannel> open_ptr(const std::string& in_session_id,
                                                          std::string*       out_error_ptr);

            /* Largest image size the channel holds. */
            static uint32_t get_max_height();
            static uint32_t get_max_width ();

            /* Bytes between rows: row size rounded up to the GPU's buffer-to-texture row alignment. */
            static uint32_t get_pitch(const uint32_t& in_width);

        private:
            struct Header;


            FrameChannel(const HANDLE& in_mapping,
                         const HANDLE& in_pace_event,
                         Header*       in_header_ptr);

            uint8_t* get_slot_pixel_ptr(const uint32_t& in_n_slot) const;

            /* Waits until the performance counter (in_frequency ticks per second) passes in_target_qpc. */
            void wait_until(const LONGLONG& in_target_qpc,
                            const LONGLONG& in_frequency);

            /* Reads a float the other process may not have set (flag + bits); false if unset. */
            static bool read_float(const volatile LONG& in_has_value,
                                   const volatile LONG& in_value_bits,
                                   float*               out_value_ptr);

            static void write_float(const float&   in_value,
                                    volatile LONG* out_value_bits_ptr,
                                    volatile LONG* out_has_value_ptr);


            Header*  m_header_ptr;
            HANDLE   m_mapping;
            uint32_t m_n_write_slot;
            HANDLE   m_pace_event; /* set by signal_pace() */
            HANDLE   m_pace_timer; /* producer's delay timer; created on first use */
        };
    }
}

#endif /* STEREO_TRANSPORT_FRAME_CHANNEL_H */

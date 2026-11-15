/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Passes images from an application to the program that shows them, through memory that the two processes share (a 32-bit
 * application can pass them to a 64-bit program). The memory has room for three images. The producer never waits for the consumer,
 * and the consumer never sees an image that is being written: the producer writes to an image that is neither the newest nor the one
 * the consumer reads, and the consumer reads the newest.
 *
 * Whichever side opens the channel first creates it. One side writes and the other reads. The channel does not check this.
 *
 * The memory layout allows a program that has the GPU import it as a heap to use it: images start at multiples of 64 KB and each
 * row starts at a multiple of 256 bytes (see get_pitch()). The two sides say how they can pass images (see set_offered_kinds() and
 * set_accepted_kinds()), and the producer uses the best way that both support.
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

            /* Producer. Returns the memory to write the image to, or nullptr if the channel has no room for the image extents. A row starts
             * one pitch (see get_pitch()) after the previous row. The consumer does not see the image until end_write().
             */
            uint8_t* begin_write_ptr(const FrameInfo& in_info);

            /* Consumer. Returns the newest image. Returns false if there is none yet. The image stays unchanged until end_read(). */
            bool begin_read(FrameInfo*      out_info_ptr,
                            const uint8_t** out_pixel_ptr_ptr);

            void end_read();

            /* Makes the image of begin_write_ptr() the newest. */
            void end_write();

            /* The kinds that the consumer takes, as it set them (see set_accepted_kinds()). None until it has set them. */
            TransportKindMask get_accepted_kinds() const;

            /* The distance at which the producer's eyes converge, in the units of the application's matrices. The consumer sets it. Returns
             * false if it has not been set, in which case the producer keeps the convergence it has. It can be set while images are passed.
             */
            bool get_convergence_distance(float* out_convergence_distance_ptr) const;

            /* How strong the 3D is to be (1 is what the converter takes to be natural, 0 is none, 2 is twice natural). The consumer sets it.
             * Returns false if it has not been set, in which case the producer keeps the eye separation it has. It can be set while images
             * are passed.
             */
            bool get_depth_strength(float* out_depth_strength_ptr) const;

            /* The eye separation that the producer is to render with. The consumer sets it. Returns false if it has not been set, in which
             * case the producer keeps its own. It can be set while images are passed.
             */
            bool get_eye_separation(float* out_eye_separation_ptr) const;

            /* Returns true if the person wants the draws that the recipe leaves alone to be pushed toward the viewer (the recipe says how far,
             * see Comfort::overlay_parallax). The consumer sets it while the images are shown. It is false until then. It can be set while
             * images are passed.
             */
            bool get_is_overlay_pushed() const;

            /* The ratio of the viewer's eye distance to the eye distance that the producer's separation is made for (1 means they are equal).
             * The consumer sets it while the images are shown. Returns false if it has not been set, in which case the producer takes it to be
             * 1. It can be set while images are passed.
             */
            bool get_ipd_scale(float* out_ipd_scale_ptr) const;

            /* The number of images that have been made the newest since the channel was created. */
            uint64_t get_n_frames_published() const;

            /* The best kind that the producer offers and the consumer takes: the one with the highest bit of both masks. NONE until
             * both have said what they can do.
             */
            TransportKind get_negotiated_kind() const;

            /* The kinds that the producer can pass images by, as it set them (see set_offered_kinds()). None until it has set them. */
            TransportKindMask get_offered_kinds() const;

            /* The largest size of the image of one eye that the producer is to render. The consumer sets it (it is the size the headset asks
             * for). Returns false if the consumer has not set it: then the images are not shown by the consumer, and the application stays
             * on the screen as it is. A size of 0 x 0 means the consumer shows the images (so the application is not shown on the screen)
             * and the producer renders at the size the application asks for. Otherwise the producer enlarges what the application renders
             * to the largest size that fits and keeps its proportions. Unlike the eye separation, this size is read once, when the
             * application creates its device, so it must be set before the application starts.
             */
            bool get_target_eye_size(uint32_t* out_width_ptr,
                                     uint32_t* out_height_ptr) const;

            /* Consumer. Sets the kinds of transport it can take. */
            void set_accepted_kinds(const TransportKindMask& in_kinds);

            /* The distance at which the eyes converge; see get_convergence_distance(). */
            void set_convergence_distance(const float& in_convergence_distance);

            /* How strong the 3D is to be; see get_depth_strength(). */
            void set_depth_strength(const float& in_depth_strength);

            /* The distance between the eyes for the producer to render with; see get_eye_separation(). */
            void set_eye_separation(const float& in_eye_separation);

            /* The viewer's eye distance, as a scale; see get_ipd_scale(). */
            void set_ipd_scale(const float& in_ipd_scale);

            /* Sets whether the draws that the recipe leaves alone are pushed; see get_is_overlay_pushed(). */
            void set_is_overlay_pushed(const bool& in_is_pushed);

            /* Producer. Sets the kinds of transport it can pass images by. */
            void set_offered_kinds(const TransportKindMask& in_kinds);

            /* Consumer. Sets how long, in microseconds, the producer waits after a signal (see signal_pace()) before it goes on. */
            void set_pace_delay_us(const uint32_t& in_delay_us);

            /* The size of the image of an eye (see get_target_eye_size()). */
            void set_target_eye_size(const uint32_t& in_width,
                                     const uint32_t& in_height);

            /* Consumer. Signals that the screen is about to want a frame. A producer that waits for the signal (see wait_for_pace()) then
             * goes on, so that it makes one frame per frame of the screen and no more.
             */
            void signal_pace();

            /* Producer. Waits until the delay that the consumer asked for has passed since the consumer's last signal. If there has been no
             * signal since the producer last called this, it first waits for one. The delay is counted from the signal, not from the moment
             * the producer receives it, so the frames are shifted in time and not made longer. Returns true if the producer had to wait.
             * Returns false if it did not wait: the consumer does not signal (it never has, or it stopped), or the producer is late (the
             * moment has passed), or the signal did not arrive within in_timeout_n_milliseconds. In each of these cases the producer goes on.
             */
            bool wait_for_pace(const uint32_t& in_timeout_n_milliseconds);


            /* Returns nullptr and sets out_error_ptr to the reason if the channel cannot be opened. The session ID names the channel:
             * processes with the same session ID share it.
             */
            static std::unique_ptr<FrameChannel> open_ptr(const std::string& in_session_id,
                                                          std::string*       out_error_ptr);

            /* The largest image size that the channel can hold. */
            static uint32_t get_max_height();
            static uint32_t get_max_width ();

            /* Returns the number of bytes from one row of an image of this width to the next: the row size rounded up to the alignment that a
             * GPU requires for the rows of a buffer that it copies to a texture.
             */
            static uint32_t get_pitch(const uint32_t& in_width);

        private:
            struct Header;


            FrameChannel(const HANDLE& in_mapping,
                         const HANDLE& in_pace_event,
                         Header*       in_header_ptr);

            uint8_t* get_slot_pixel_ptr(const uint32_t& in_n_slot) const;

            /* Waits until the system clock (counted in ticks, with in_frequency ticks per second) has passed in_target_qpc. */
            void wait_until(const LONGLONG& in_target_qpc,
                            const LONGLONG& in_frequency);

            /* Reads a value that the other process may not have set yet. The flag says whether there is a value, and the bits are those of
             * the float. Returns false if there is none.
             */
            static bool read_float(const volatile LONG& in_has_value,
                                   const volatile LONG& in_value_bits,
                                   float*               out_value_ptr);

            static void write_float(const float&   in_value,
                                    volatile LONG* out_value_bits_ptr,
                                    volatile LONG* out_has_value_ptr);


            Header*  m_header_ptr;
            HANDLE   m_mapping;
            uint32_t m_n_write_slot;
            HANDLE   m_pace_event; /* set by the consumer when the screen is about to want a frame */
            HANDLE   m_pace_timer; /* the timer for the producer's wait for the delay; created when first needed */
        };
    }
}

#endif /* STEREO_TRANSPORT_FRAME_CHANNEL_H */

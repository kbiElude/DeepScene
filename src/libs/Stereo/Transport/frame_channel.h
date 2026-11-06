/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Images from an application to the program which shows them, through memory which the two processes share (a 32-bit
 * application can pass them to a 64-bit program). There is room for three images, so that the producer never waits for the
 * consumer and the consumer never sees an image which is being written: the producer writes one which is neither the newest nor
 * the one the consumer reads, and the consumer reads the newest.
 *
 * Whichever side opens the channel first makes it. One side writes and the other reads: the channel does not check that.
 *
 * The memory is laid out so that a program which has the GPU import it as a heap can use it: the images start at multiples of 64 KB
 * and each row starts at a multiple of 256 bytes (see get_pitch()). The two sides say how they can pass images (see
 * set_offered_kinds() and set_accepted_kinds()), and the producer uses the best way that both can.
 */
#ifndef STEREO_TRANSPORT_FRAME_CHANNEL_H
#define STEREO_TRANSPORT_FRAME_CHANNEL_H

#include "Stereo/Transport/types.h"
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

            /* The producer: the memory to write the image to, which has room for the extents; nullptr if it has not. A row starts a
             * pitch (see get_pitch()) after the one before. The image is not seen until end_write().
             */
            uint8_t* begin_write_ptr(const FrameInfo& in_info);

            /* The consumer: the newest image. Returns false if there is none yet. The image stays as it is until end_read(). */
            bool begin_read(FrameInfo*      out_info_ptr,
                            const uint8_t** out_pixel_ptr_ptr);

            void end_read();

            /* Makes the image of begin_write_ptr() the newest. */
            void end_write();

            /* What the consumer takes, as the consumer said (see set_accepted_kinds()); none until it has. */
            TransportKindMask get_accepted_kinds() const;

            /* The distance at which the eyes of the producer converge, in the units of the application's matrices, which the consumer
             * sets; false if it has not been set, and they do not converge. It can be set while images are passed.
             */
            bool get_convergence_distance(float* out_convergence_distance_ptr) const;

            /* How strong the 3D is to be (1 is what the converter takes to be natural, 0 is none, 2 is twice that), which the consumer
             * sets; false if it has not been set, and the producer keeps the distance between the eyes it has. It can be set while images
             * are passed.
             */
            bool get_depth_strength(float* out_depth_strength_ptr) const;

            /* The distance between the eyes that the producer is to render with, which the consumer sets; false if it has not been set,
             * and the producer keeps its own. It can be set while images are passed.
             */
            bool get_eye_separation(float* out_eye_separation_ptr) const;

            /* Whether the person wants the draws that the recipe leaves alone to be pushed toward the viewer (see Comfort::overlay_parallax in the
             * recipe, which says how far), which the consumer sets while the images are shown; false until it says so. It can be set while images
             * are passed.
             */
            bool get_is_overlay_pushed() const;

            /* How the distance between the eyes of the person who looks compares with the one that the producer's separation is made for (1 is
             * the same), which the consumer sets while the images are shown; false if it has not been set, and the producer takes it to be 1. It
             * can be set while images are passed.
             */
            bool get_ipd_scale(float* out_ipd_scale_ptr) const;

            /* How many images have been made the newest, since the channel was made. */
            uint64_t get_n_frames_published() const;

            /* The best kind that the producer offers and the consumer takes: the one with the highest bit of both masks. NONE until
             * both have said what they can do.
             */
            TransportKind get_negotiated_kind() const;

            /* What the producer can pass images by, as the producer said (see set_offered_kinds()); none until it has. */
            TransportKindMask get_offered_kinds() const;

            /* The size of the image of an eye that the producer is to render at most, which the consumer sets (the size the headset asks for);
             * false if it has not been set, and the producer renders as the application asks. The producer makes what the application renders
             * as large as fits that size with its proportions. Unlike the distance between the eyes it is read once, when the application
             * makes its device, so it is to be set before the application starts.
             */
            bool get_target_eye_size(uint32_t* out_width_ptr,
                                     uint32_t* out_height_ptr) const;

            /* Said by the consumer: the kinds of transport it can take. */
            void set_accepted_kinds(const TransportKindMask& in_kinds);

            /* The distance at which the eyes converge; see get_convergence_distance(). */
            void set_convergence_distance(const float& in_convergence_distance);

            /* How strong the 3D is to be; see get_depth_strength(). */
            void set_depth_strength(const float& in_depth_strength);

            /* The distance between the eyes for the producer to render with; see get_eye_separation(). */
            void set_eye_separation(const float& in_eye_separation);

            /* The distance between the eyes of the person who looks, as a scale; see get_ipd_scale(). */
            void set_ipd_scale(const float& in_ipd_scale);

            /* Whether the draws that the recipe leaves alone are pushed; see get_is_overlay_pushed(). */
            void set_is_overlay_pushed(const bool& in_is_pushed);

            /* Said by the producer: the kinds of transport it can pass images by. */
            void set_offered_kinds(const TransportKindMask& in_kinds);

            /* The consumer: how long after a signal (see signal_pace()) the producer is to wait, in microseconds, before it goes on. */
            void set_pace_delay_us(const uint32_t& in_delay_us);

            /* The size of the image of an eye (see get_target_eye_size()). */
            void set_target_eye_size(const uint32_t& in_width,
                                     const uint32_t& in_height);

            /* The consumer: the screen is about to want a frame. A producer which waits for it (see wait_for_pace()) goes on, so that it makes
             * one frame for each frame of the screen, and not more.
             */
            void signal_pace();

            /* The producer: waits until the delay that the consumer asked for has passed since its last signal, the signal first if there has
             * not been one since the producer was here last. The delay is counted from the signal and not from the moment the producer gets
             * it, so that it moves the frames in time and does not make them longer. Returns true if the producer had to wait, and false if
             * it did not: the consumer does not signal (it has not, or has stopped), or the producer is late (the moment is past), or the
             * signal does not come in the time, and the producer goes on.
             */
            bool wait_for_pace(const uint32_t& in_timeout_n_milliseconds);


            /* Returns nullptr, and says why, if the channel cannot be opened. The session names it: processes with the same one
             * share it.
             */
            static std::unique_ptr<FrameChannel> open_ptr(const std::string& in_session_id,
                                                          std::string*       out_error_ptr);

            /* What an image can be. */
            static uint32_t get_max_height();
            static uint32_t get_max_width ();

            /* The bytes from a row of an image of the width to the next: the row, rounded up to what a GPU takes for a row of a buffer
             * that it copies to a texture.
             */
            static uint32_t get_pitch(const uint32_t& in_width);

        private:
            struct Header;


            FrameChannel(const HANDLE& in_mapping,
                         const HANDLE& in_pace_event,
                         Header*       in_header_ptr);

            uint8_t* get_slot_pixel_ptr(const uint32_t& in_n_slot) const;

            /* Waits until the time on the clock of the system (in ticks of it per second), which is then past. */
            void wait_until(const LONGLONG& in_target_qpc,
                            const LONGLONG& in_frequency);


            Header*  m_header_ptr;
            HANDLE   m_mapping;
            uint32_t m_n_write_slot;
            HANDLE   m_pace_event; /* set by the consumer when the screen is about to want a frame */
            HANDLE   m_pace_timer; /* of the producer's wait for the delay; made when it is needed  */
        };
    }
}

#endif /* STEREO_TRANSPORT_FRAME_CHANNEL_H */

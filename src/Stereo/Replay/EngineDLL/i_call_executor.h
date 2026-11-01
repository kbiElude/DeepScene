/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_I_CALL_EXECUTOR_H
#define STEREO_REPLAY_ENGINEDLL_I_CALL_EXECUTOR_H

#include "Stereo/Replay/EngineDLL/types.h"
#include "i_chunk_source.h"
#include "types.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Replays the calls of one API group (see Capture::APIDumpLoader::APIGroup). */
            class ICallExecutor
            {
            public:
                virtual ~ICallExecutor() = default;

                /* Returns false (and says why) if the call cannot be replayed. The chunk source provides the call's data chunks. */
                virtual bool execute(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     const IChunkSource&                          in_chunk_source,
                                     std::string*                                 out_error_ptr) = 0;

                /* When the call is executed, if its frame is executed more than once. */
                virtual ExecutionClass get_execution_class(const Capture::APIDumpLoader::DumpedAPICall& in_call) const
                {
                    (void) in_call;

                    return ExecutionClass::FIRST_TIME_ONLY;
                }

                /* A frame of the workload has been replayed. */
                virtual void on_frame_presented() = 0;

                /* The replay is not running (paused, or at the end), and called now and then: a window needs its messages
                 * answered all the time, or the system takes it for hung.
                 */
                virtual void on_idle()
                {
                    /* Stub */
                }

                /* Reads the back buffer of the device, once, into what is asked for (a null pointer is not asked for): the pixels as the
                 * device has them (rows one after the other with no padding; what write_back_buffer() puts back), and the image
                 * (see FrameImage, valid until the next call). Returns false if there is no back buffer or its format is not handled.
                 */
                virtual bool read_back_buffer(std::vector<uint8_t>* out_pixel_u8_vec_ptr,
                                              FrameImage*           out_image_ptr)
                {
                    (void) out_image_ptr;
                    (void) out_pixel_u8_vec_ptr;

                    return false;
                }

                /* The replay goes back to the start: forget the objects which the replayed calls created. Windows which the
                 * viewer sees may stay, to be used again.
                 */
                virtual void reset() = 0;

                /* Puts the state of the device back as save_state() found it. */
                virtual void restore_state()
                {
                    /* Stub */
                }

                /* Remembers the state of the device, at the start of a frame that is executed more than once. */
                virtual void save_state()
                {
                    /* Stub */
                }

                /* Whether the windows of the replayed application are shown. They are, unless this says otherwise (before the replay is
                 * played). The application still renders to them.
                 */
                virtual void set_windows_visible(const bool& in_are_visible);

                /* The replay is over: release everything. Unless the executor keeps something for reset(), that is the same. */
                virtual void shutdown();

                /* Puts pixels that read_back_buffer() gave into the back buffer. Returns false if that cannot be done. */
                virtual bool write_back_buffer(const std::vector<uint8_t>& in_pixel_u8_vec)
                {
                    (void) in_pixel_u8_vec;

                    return false;
                }
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_I_CALL_EXECUTOR_H */

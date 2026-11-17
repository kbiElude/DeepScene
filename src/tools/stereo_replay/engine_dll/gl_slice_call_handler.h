/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_GL_SLICE_CALL_HANDLER_H
#define STEREO_REPLAY_ENGINEDLL_GL_SLICE_CALL_HANDLER_H

#include "gl_display_list_tracker.h"
#include "i_slice_call_handler.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* For OpenGL. Besides its content calls, a dead frame leaves out the calls of a display list that only draws, and the calls that
             * a glPopAttrib undoes (see GLAttributeCoverage). The lists and the pushes are tracked here, and nowhere else.
             */
            class GLSliceCallHandler : public ISliceCallHandler
            {
            public:
                GLSliceCallHandler();

                Capture::APIDumpLoader::CallKeptFunc create_call_kept_func        (const FrameLiveFunc&                        in_is_live_func) const override;
                Capture::APIDumpLoader::APIGroup     get_group                    ()                                                            const override;
                bool                                 is_chunk_wanted_in_dead_frame(const Capture::APIDumpLoader::DumpedAPICall& in_call)      const override;
                bool                                 should_execute_call          (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                   const bool&                                  in_is_dead_frame) override;

            private:
                /* Whether a call of a list in a dead frame is left out: it is if the list only draws. A list that depends on the location of
                 * the eye ends the process (AI_ASSERT_ALWAYS): what it does to the matrices and the viewport would not be done.
                 */
                bool is_list_call_skippable(const Capture::APIDumpLoader::DumpedAPICall& in_call) const;

                GLDisplayListTracker m_list_tracker;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_GL_SLICE_CALL_HANDLER_H */

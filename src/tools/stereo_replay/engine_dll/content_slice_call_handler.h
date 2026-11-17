/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_CONTENT_SLICE_CALL_HANDLER_H
#define STEREO_REPLAY_ENGINEDLL_CONTENT_SLICE_CALL_HANDLER_H

#include "i_slice_call_handler.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* For an API whose dead frames only leave out their content calls (see CallClass), as Direct3D does. */
            class ContentSliceCallHandler : public ISliceCallHandler
            {
            public:
                explicit ContentSliceCallHandler(const Capture::APIDumpLoader::APIGroup& in_group);

                Capture::APIDumpLoader::CallKeptFunc create_call_kept_func        (const FrameLiveFunc&                        in_is_live_func) const override;
                Capture::APIDumpLoader::APIGroup     get_group                    ()                                                            const override;
                bool                                 is_chunk_wanted_in_dead_frame(const Capture::APIDumpLoader::DumpedAPICall& in_call)      const override;
                bool                                 should_execute_call          (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                   const bool&                                  in_is_dead_frame) override;

            private:
                Capture::APIDumpLoader::APIGroup m_group;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_CONTENT_SLICE_CALL_HANDLER_H */

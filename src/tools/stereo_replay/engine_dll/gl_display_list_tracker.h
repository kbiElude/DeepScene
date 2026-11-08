/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_GL_DISPLAY_LIST_TRACKER_H
#define STEREO_REPLAY_ENGINEDLL_GL_DISPLAY_LIST_TRACKER_H

#include "analysis/api_dump_analysis/frontends/OpenGL/types.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include <map>
#include <stdint.h>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Follows the display lists that the calls of a replay make, in the order they are replayed in, and says what each does to what an eye has
             * of its own (see GlListEyeDependence). The slice skipper asks it whether a call of a list may be left out of a dead frame.
             */
            class GlDisplayListTracker
            {
            public:
                GlDisplayListTracker();

                /* The class of the list, if it is defined (not found: the list was never made, or was deleted). */
                bool find_eye_dependence(const uint32_t&                                            in_list,
                                         Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence* out_dependence_ptr) const;

                /* Whether a list is being made: its calls are kept and not made, so they are not the calls of a frame to leave out. */
                bool is_recording() const;

                /* Every call, in the order of the replay. */
                void on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call);

            private:
                std::map<uint32_t, Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence> m_dependence_map;
                Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence                     m_recording_dependence;
                bool                                                                          m_is_recording;
                uint32_t                                                                      m_recording_list;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_GL_DISPLAY_LIST_TRACKER_H */

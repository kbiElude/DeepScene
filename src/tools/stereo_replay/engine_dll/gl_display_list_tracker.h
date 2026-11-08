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

                /* Whether the list does anything that is not content (see SliceSkipper::is_content_call()): sets state, including the values that the next vertices
                 * have, or calls a list that does, or one that is not defined. A call of such a list cannot be left out of a dead frame, since the frames after it go
                 * on from the state it leaves. Not found: the list was never made, or was deleted.
                 */
                bool find_sets_state(const uint32_t& in_list,
                                     bool*           out_sets_state_ptr) const;

                /* Whether a list is being made: its calls are kept and not made, so they are not the calls of a frame to leave out. */
                bool is_recording() const;

                /* Every call, in the order of the replay. */
                void on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call);

            private:
                /* What a list that is defined does. */
                struct ListEffects
                {
                    Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence dependence = Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence::INDEPENDENT;
                    bool                                                      sets_state = false;
                };

                std::map<uint32_t, ListEffects> m_effects_map;
                bool                            m_is_recording;
                ListEffects                     m_recording_effects;
                uint32_t                        m_recording_list;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_GL_DISPLAY_LIST_TRACKER_H */

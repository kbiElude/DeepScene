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
            /* Tracks the display lists that the replayed calls create, in replay order, and records what each list does to the state
             * that is specific to an eye (see GLListEyeDependence). The slice skipper asks it whether a call to a list may be left out
             * of a dead frame.
             */
            class GLDisplayListTracker
            {
            public:
                GLDisplayListTracker();

                /* Returns the eye-dependence class of the list. Returns false if the list is not defined (it was never created, or it
                 * was deleted).
                 */
                bool find_eye_dependence(const uint32_t&                                            in_list,
                                         Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence* out_dependence_ptr) const;

                /* Returns whether the list does anything that is not content (see SliceSkipper::is_content_call()). A list does so if it
                 * sets state, including the values the next vertices take (such as the current color), if it calls a list that does, or if
                 * it calls an undefined list. A call to such a list cannot be left out of a dead frame, because later frames continue from
                 * the state it leaves. Returns false if the list is not defined (it was never created, or it was deleted).
                 */
                bool find_sets_state(const uint32_t& in_list,
                                     bool*           out_sets_state_ptr) const;

                /* Whether a list is being recorded. Its calls are stored in the list instead of being executed, so they are not calls
                 * of a frame that could be left out.
                 */
                bool is_recording() const;

                /* Receives every call, in replay order. */
                void on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call);

            private:
                /* What a defined list does. */
                struct ListEffects
                {
                    Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence dependence = Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::INDEPENDENT;
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

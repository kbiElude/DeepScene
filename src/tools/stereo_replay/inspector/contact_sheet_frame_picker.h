/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Picks the frames that a contact sheet shows.
 */
#ifndef STEREO_REPLAY_INSPECTOR_CONTACT_SHEET_FRAME_PICKER_H
#define STEREO_REPLAY_INSPECTOR_CONTACT_SHEET_FRAME_PICKER_H

#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace Inspector
        {
            class ContactSheetFramePicker
            {
            public:
                /* Chooses frames spread evenly from the first to the last, in order. Both ends are included, except when only one frame is requested: then
                 * it is the first. If the range has fewer frames than requested, all of them are returned. The result is empty if no frames are requested
                 * or the first frame is after the last.
                 */
                static std::vector<uint32_t> sample(const uint32_t& in_first_frame,
                                                    const uint32_t& in_last_frame,
                                                    const uint32_t& in_n_frames);
            };
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_CONTACT_SHEET_FRAME_PICKER_H */

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes the image of a frame, or the images of several frames on one sheet.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_SCREENSHOT_MODE_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_SCREENSHOT_MODE_H

#include "types.h"
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            /* Replays the workload once, up to the last frame of the contact sheet, and writes the sheet. The sheet shows frames spread evenly from
             * the first frame of the options to the last (or to the end of the workload), each rendered as a screenshot would be.
             */
            bool take_contact_sheet(const Options& in_options,
                                    std::string*   out_error_ptr);

            /* Replays the workload once, up to the screenshot frame, and writes its image to the screenshot file. With a recipe the image shows the
             * eye view that the options ask for; without one it shows the frame as it is.
             */
            bool take_screenshot(const Options& in_options,
                                 std::string*   out_error_ptr);
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_SCREENSHOT_MODE_H */

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
            /* Replays the workload once, up to the last of the frames that the options have a contact sheet of, and writes the sheet: the frames spread evenly
             * from the first frame of the options to the last (or the end of the workload), each as a screenshot is.
             */
            bool take_contact_sheet(const Options& in_options,
                                    std::string*   out_error_ptr);

            /* Replays the workload once, up to the screenshot frame, and writes the image of it to the screenshot file: with a recipe the eyes as the options
             * say, otherwise the frame as it is.
             */
            bool take_screenshot(const Options& in_options,
                                 std::string*   out_error_ptr);
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_SCREENSHOT_MODE_H */

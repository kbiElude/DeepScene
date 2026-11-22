/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes the image of a frame, or the images of several frames on one sheet.
 */
#ifndef STEREO_REPLAY_INSPECTOR_SCREENSHOT_MODE_H
#define STEREO_REPLAY_INSPECTOR_SCREENSHOT_MODE_H

#include "types.h"
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace Inspector
        {
            /* Replays the workload once, up to the last frame of the contact sheet, and writes the sheet. The sheet shows frames spread evenly from
             * the first frame of the options to the last (or to the end of the workload), each rendered as a screenshot would be.
             */
            bool take_contact_sheet(const Options& in_options,
                                    std::string*   out_error_ptr);

            /* Replays the workload once, up to the last of the screenshot frames, and writes the image of each to the screenshot file (one frame) or to files
             * that have the number of the frame in the name (several; see CommandLine::make_numbered_filename()). With a recipe the image shows the eye view that the
             * options ask for; without one it shows the frame as it is. A frame that the replay does not reach is not written and is in the report. Returns false, with
             * the reason, if the replay fails, a file cannot be written, or none of the frames was reached.
             */
            bool take_screenshot(const Options&    in_options,
                                 ScreenshotReport* out_report_ptr,
                                 std::string*      out_error_ptr);
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_SCREENSHOT_MODE_H */

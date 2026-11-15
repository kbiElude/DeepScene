/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Makes the overlay report of the options, in this process or shared between processes.
 */
#ifndef STEREO_REPLAY_INSPECTOR_OVERLAY_REPORT_MODE_H
#define STEREO_REPLAY_INSPECTOR_OVERLAY_REPORT_MODE_H

#include "types.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace Inspector
        {
            /* Starts the parts of an overlay report, each as a process of this program, and waits for them (see OverlayReport::RunShardsFunc). */
            bool run_report_shards(const Options&                  in_options,
                                  const uint32_t&                 in_n_shards,
                                  const std::vector<std::string>& in_result_filename_vec,
                                  std::string*                    out_error_ptr);

            /* Makes the report of the overlay candidates of the options (see OverlayReport). */
            bool take_overlay_report(const Options& in_options,
                                    std::string*   out_error_ptr);
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_OVERLAY_REPORT_MODE_H */

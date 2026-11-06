/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Makes the overlay probe of the options, in this process or shared between processes.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_PROBE_MODE_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_PROBE_MODE_H

#include "types.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            /* Starts the parts of an overlay probe, each as a process of this program, and waits for them (see OverlayProbe::RunShardsFunc). */
            bool run_probe_shards(const Options&                  in_options,
                                  const uint32_t&                 in_n_shards,
                                  const std::vector<std::string>& in_result_filename_vec,
                                  std::string*                    out_error_ptr);

            /* Makes the probe of the overlay candidates of the options (see OverlayProbe). */
            bool take_overlay_probe(const Options& in_options,
                                    std::string*   out_error_ptr);
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_PROBE_MODE_H */

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Compares replays: the ways with the frames as they are, and a workload with another one.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_COMPARISON_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_COMPARISON_H

#include "frame_hash_sink.h"
#include "types.h"
#include "utils/images/types.h"
#include <map>
#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            /* Replays the frames of the options from the workload of the options and from the other workload, as they are, and says whether the images are the same.
             * Returns false if a replay fails. A slice of the workload (frames 0 to B) is the same as the workload for the frames up to B.
             */
            bool compare_workloads(const Options& in_options,
                                   bool*          out_has_difference_ptr,
                                   std::string*   out_error_ptr);

            /* Writes the images of one frame, as it is and executed twice, to the directory of the options. */
            void dump_frame(const Options&  in_options,
                            const uint32_t& in_n_frame,
                            const Variant&  in_variant);

            /* Says how the executions of a way of replaying differ from the frames as they are. Returns the first frame in which one of the
             * executions differs, and false if there is none.
             */
            bool print_comparison(const Variant&                                     in_variant,
                                  const std::map<FrameHashSink::ImageKey, uint64_t>& in_baseline_map,
                                  const std::map<FrameHashSink::ImageKey, uint64_t>& in_hash_map,
                                  uint32_t*                                          out_first_frame_ptr);

            /* Says, and writes an image of, how the image of an execution differs from the one of the frame as it is. */
            void write_difference(const std::string&          in_label,
                                  const std::string&          in_path,
                                  const Utils::Images::Image& in_image_as_it_is,
                                  const Utils::Images::Image& in_image_of_execution);
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_COMPARISON_H */

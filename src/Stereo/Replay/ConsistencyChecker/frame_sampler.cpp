/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_sampler.h"

std::vector<uint32_t> Stereo::Replay::ConsistencyChecker::FrameSampler::sample(const uint32_t& in_first_frame,
                                                                               const uint32_t& in_last_frame,
                                                                               const uint32_t& in_n_frames)
{
    std::vector<uint32_t> result_vec;

    if (in_n_frames    == 0 ||
        in_first_frame >  in_last_frame)
    {
        goto end;
    }

    if (in_n_frames == 1)
    {
        result_vec.push_back(in_first_frame);

        goto end;
    }

    if (static_cast<uint64_t>(in_last_frame) - in_first_frame + 1 <= in_n_frames)
    {
        for (uint64_t n_frame = in_first_frame;
                      n_frame <= in_last_frame;
                    ++n_frame)
        {
            result_vec.push_back(static_cast<uint32_t>(n_frame) );
        }

        goto end;
    }

    for (uint32_t n_sample = 0;
                  n_sample < in_n_frames;
                ++n_sample)
    {
        result_vec.push_back(in_first_frame + static_cast<uint32_t>(static_cast<uint64_t>(in_last_frame - in_first_frame) * n_sample / (in_n_frames - 1) ) );
    }

end:
    return result_vec;
}

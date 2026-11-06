/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_detail_marker.h"

Analysis::APIDumpAnalysis::FrameDetail Tests::FrameDetailMarker::make(const uint32_t& in_marker)
{
    Analysis::APIDumpAnalysis::FrameDetail result;
    Analysis::APIDumpAnalysis::PassInstance instance;

    instance.structure_id = in_marker;

    result.pass_instance_vec.push_back(instance);

    return result;
}

uint32_t Tests::FrameDetailMarker::read(const Analysis::APIDumpAnalysis::FrameDetail& in_detail)
{
    return in_detail.pass_instance_vec.at(0).structure_id;
}

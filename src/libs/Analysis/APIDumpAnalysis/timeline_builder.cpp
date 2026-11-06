/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/timeline_builder.h"
#include <algorithm>

Analysis::APIDumpAnalysis::Timeline Analysis::APIDumpAnalysis::TimelineBuilder::build(const AnalysisResult& in_analysis)
{
    const auto&    model_ref     = in_analysis.model;
    const double   qpc_frequency = static_cast<double>(in_analysis.summary.qpc_frequency);
    Timeline       result;

    const auto get_seconds_func = [&](const uint64_t& in_timestamp_qpc)
    {
        double result_seconds = 0.0;

        if (qpc_frequency    >  0.0                                     &&
            in_timestamp_qpc >= in_analysis.summary.first_timestamp_qpc)
        {
            result_seconds = static_cast<double>(in_timestamp_qpc - in_analysis.summary.first_timestamp_qpc) / qpc_frequency;
        }

        return result_seconds;
    };

    result.pass_usage_vec.resize (model_ref.pass_structure_interner.get_n_structures() );
    result.shape_usage_vec.resize(model_ref.frame_shape_interner.get_n_structures   () );

    for (uint32_t n_pass = 0;
                  n_pass < result.pass_usage_vec.size();
                ++n_pass)
    {
        result.pass_usage_vec.at(n_pass).pass_structure_id = n_pass;
    }

    for (uint32_t n_shape = 0;
                  n_shape < result.shape_usage_vec.size();
                ++n_shape)
    {
        result.shape_usage_vec.at(n_shape).shape_id = n_shape;
    }

    for (const auto& frame : model_ref.frame_record_vec)
    {
        auto& shape_usage = result.shape_usage_vec.at(frame.shape_id);

        if (result.run_vec.empty()                            ||
            result.run_vec.back ().shape_id != frame.shape_id)
        {
            Run run;

            run.begin_seconds     = get_seconds_func(frame.begin_timestamp_qpc);
            run.first_frame_index = frame.frame_index;
            run.shape_id          = frame.shape_id;

            result.run_vec.push_back(run);

            ++shape_usage.n_runs;
        }

        if (shape_usage.n_frames == 0)
        {
            shape_usage.first_seconds = get_seconds_func(frame.begin_timestamp_qpc);
        }

        shape_usage.last_seconds = get_seconds_func(frame.end_timestamp_qpc);

        ++shape_usage.n_frames;

        result.run_vec.back().end_seconds  = get_seconds_func(frame.end_timestamp_qpc);
        result.run_vec.back().n_draws     += frame.n_draws;

        ++result.run_vec.back().n_frames;
    }

    for (auto& run : result.run_vec)
    {
        if (run.end_seconds > run.begin_seconds)
        {
            run.fps = static_cast<double>(run.n_frames) / (run.end_seconds - run.begin_seconds);
        }
    }

    for (uint32_t n_shape = 0;
                  n_shape < result.shape_usage_vec.size();
                ++n_shape)
    {
        const auto& shape = model_ref.frame_shape_interner.get_structure(n_shape);

        /* IDs are sorted, so the instances of a pass structure are adjacent. */
        for (size_t n_id = 0;
                    n_id < shape.pass_structure_id_vec.size();
                  ++n_id)
        {
            auto& pass_usage = result.pass_usage_vec.at(shape.pass_structure_id_vec.at(n_id) );

            pass_usage.n_instances += result.shape_usage_vec.at(n_shape).n_frames;

            if (n_id                                 == 0                                        ||
                shape.pass_structure_id_vec.at(n_id) != shape.pass_structure_id_vec.at(n_id - 1))
            {
                pass_usage.n_frames += result.shape_usage_vec.at(n_shape).n_frames;
            }
        }
    }

    result.setup_frame_index_vec = get_setup_frame_index_vec(model_ref);

    return result;
}

std::vector<uint64_t> Analysis::APIDumpAnalysis::TimelineBuilder::get_setup_frame_index_vec(const Model& in_model)
{
    constexpr uint64_t SETUP_DURATION_FACTOR = 20; /* multiple of the median frame duration */

    std::vector<uint64_t> duration_vec;
    uint64_t              median_duration = 0;
    std::vector<uint64_t> result_vec;

    for (const auto& frame : in_model.frame_record_vec)
    {
        duration_vec.push_back(frame.end_timestamp_qpc - frame.begin_timestamp_qpc);
    }

    if (duration_vec.empty() == false)
    {
        const auto median_iterator = duration_vec.begin() + duration_vec.size() / 2;

        std::nth_element(duration_vec.begin(),
                         median_iterator,
                         duration_vec.end() );

        median_duration = *median_iterator;
    }

    for (const auto& frame : in_model.frame_record_vec)
    {
        if (median_duration                                     > 0                                       &&
            frame.end_timestamp_qpc - frame.begin_timestamp_qpc > SETUP_DURATION_FACTOR * median_duration)
        {
            result_vec.push_back(frame.frame_index);
        }
    }

    return result_vec;
}

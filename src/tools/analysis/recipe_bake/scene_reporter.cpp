/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/convergence_model.h"
#include "recipe_baker.h"
#include "scene_reporter.h"
#include <map>

std::vector<Analysis::RecipeBake::SceneReport> Analysis::RecipeBake::SceneReporter::find(const Analysis::Recipe::Recipe&                  in_recipe,
                                                                                         const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    std::map<uint32_t, SceneReport> report_map;
    std::vector<SceneReport>        result_vec;

    for (const auto& measure : RecipeBaker::measure_scenes(in_recipe,
                                                           in_analysis) )
    {
        if (measure.has_depth)
        {
            SceneReport report;

            report.limits  = RecipeBaker::resolve_scene_limits(in_recipe,
                                                               measure);
            report.measure = measure;

            report_map[measure.shape_id] = report;
        }
    }

    /* Where the frames of each scene are, as runs: a run is a stretch of consecutive frames of the scene. */
    for (const auto& frame : in_analysis.model.frame_record_vec)
    {
        const auto report_iterator = report_map.find(frame.shape_id);

        if (report_iterator != report_map.end() )
        {
            auto& report = report_iterator->second;

            if (report.run_vec.empty()                                                  ||
                report.run_vec.back().last_frame_index + 1 != frame.frame_index)
            {
                FrameRun run;

                run.first_frame_index = frame.frame_index;
                run.last_frame_index  = frame.frame_index;

                report.run_vec.push_back(run);
            }
            else
            {
                report.run_vec.back().last_frame_index = frame.frame_index;
            }

            report.n_frames++;
        }
    }

    for (auto& report_entry : report_map)
    {
        auto&       report  = report_entry.second;
        const float scale   = (report.measure.projection_scale > 0.0f) ? report.measure.projection_scale
                                                                       : 1.0f;
        const float half    = 0.5f;

        /* A model that has no value yet moves to its first target at once, whatever the fraction. */
        const float reference_frame_seconds = Stereo::Core::ConvergenceModel::get_reference_frame_seconds();
        const float fraction_of_the_way     = Stereo::Core::ConvergenceModel::get_fraction_of_the_way(reference_frame_seconds);

        /* The same model that the converter uses, with the strength 1 that the player starts with and the limit of the scene. */
        Stereo::Core::ConvergenceModel limited_model;
        Stereo::Core::ConvergenceModel natural_model;

        limited_model.set_depth_strength               (1.0f);
        limited_model.follow_scene_convergence_distance(report.limits.distance,
                                                        fraction_of_the_way);
        limited_model.follow_scene_max_separation      (report.limits.max_separation,
                                                        fraction_of_the_way);
        natural_model.set_depth_strength               (1.0f);
        natural_model.follow_scene_convergence_distance(report.limits.distance,
                                                        fraction_of_the_way);

        report.natural_separation = natural_model.get_effective_eye_separation();
        report.separation         = limited_model.get_effective_eye_separation();

        if (report.measure.near_depth > 0.0f                   &&
            report.measure.near_depth < report.limits.distance)
        {
            report.near_parallax = report.separation * scale * (1.0f / report.measure.near_depth - 1.0f / report.limits.distance) * half;
        }

        if (report.measure.far_depth > report.limits.distance)
        {
            report.far_parallax = report.separation * scale * (1.0f / report.limits.distance - 1.0f / report.measure.far_depth) * half;
        }

        result_vec.push_back(report);
    }

    return result_vec;
}

void Analysis::RecipeBake::SceneReporter::write_json(const std::vector<SceneReport>& in_report_vec,
                                                     const float&                    in_screen_degrees,
                                                     Utils::Json::JsonWriter*        inout_writer_ptr)
{
    inout_writer_ptr->begin_array();

    for (const auto& report : in_report_vec)
    {
        inout_writer_ptr->begin_object();

        inout_writer_ptr->write_key    ("shape");
        inout_writer_ptr->write_hex_u64(report.measure.shape_key);

        inout_writer_ptr->write_key("n_frames");
        inout_writer_ptr->write_u64(report.n_frames);

        inout_writer_ptr->write_key   ("runs");
        inout_writer_ptr->begin_array ();

        for (const auto& run : report.run_vec)
        {
            inout_writer_ptr->begin_array();
            inout_writer_ptr->write_u64  (run.first_frame_index);
            inout_writer_ptr->write_u64  (run.last_frame_index);
            inout_writer_ptr->end_array  ();
        }

        inout_writer_ptr->end_array();

        inout_writer_ptr->write_key    ("distance");
        inout_writer_ptr->write_double (static_cast<double>(report.limits.distance) );
        inout_writer_ptr->write_key    ("near_depth");
        inout_writer_ptr->write_double (static_cast<double>(report.measure.near_depth) );
        inout_writer_ptr->write_key    ("far_depth");
        inout_writer_ptr->write_double (static_cast<double>(report.measure.far_depth) );
        inout_writer_ptr->write_key    ("scale");
        inout_writer_ptr->write_double (static_cast<double>(report.measure.projection_scale) );
        inout_writer_ptr->write_key    ("limit");
        inout_writer_ptr->write_double (static_cast<double>(report.limits.max_separation) );
        inout_writer_ptr->write_key    ("limited_by");
        inout_writer_ptr->write_string ((report.limits.max_separation <= 0.0f) ? "nothing"
                                      : (report.limits.is_overridden && report.limits.is_far_limit == false) ? "near or the recipe's scene"
                                      : (report.limits.is_far_limit)                                         ? "far"
                                                                                                             : "near");
        inout_writer_ptr->write_key    ("overridden");
        inout_writer_ptr->write_bool   (report.limits.is_overridden);
        inout_writer_ptr->write_key    ("natural_separation");
        inout_writer_ptr->write_double (static_cast<double>(report.natural_separation) );
        inout_writer_ptr->write_key    ("separation");
        inout_writer_ptr->write_double (static_cast<double>(report.separation) );
        inout_writer_ptr->write_key    ("near_parallax");
        inout_writer_ptr->write_double (static_cast<double>(report.near_parallax) );
        inout_writer_ptr->write_key    ("far_parallax");
        inout_writer_ptr->write_double (static_cast<double>(report.far_parallax) );
        inout_writer_ptr->write_key    ("near_degrees");
        inout_writer_ptr->write_double (static_cast<double>(report.near_parallax * in_screen_degrees) );
        inout_writer_ptr->write_key    ("far_degrees");
        inout_writer_ptr->write_double (static_cast<double>(report.far_parallax * in_screen_degrees) );

        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
}

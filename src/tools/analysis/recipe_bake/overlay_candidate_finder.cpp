/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_commands.h"
#include "analysis/api_dump_analysis/timeline_builder.h"
#include "overlay_candidate_finder.h"
#include "common/macros.h"
#include <algorithm>
#include <array>
#include <set>
#include <string.h>

std::vector<Analysis::RecipeBake::OverlayCandidate> Analysis::RecipeBake::OverlayCandidateFinder::find(const Analysis::Recipe::Recipe&                  in_recipe,
                                                                                                       const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    /* The part of the picture that the box around a draw has to cover for the draw to be called one that covers the view: a full quad of a picture that has bars above
     * and below it (as Gerbera's does) is 0.79 of the viewport.
     */
    constexpr float COVERS_VIEW_FRACTION = 0.75f;

    const Analysis::Recipe::RecipeApplier applier(in_recipe);

    const auto&                 model_ref          = in_analysis.model;
    const std::vector<uint32_t> static_pass_id_vec = Analysis::APIDumpAnalysis::QueryCommands::get_static_camera_pass_id_vec(in_analysis);

    const std::set<uint32_t> static_pass_id_set     (static_pass_id_vec.begin(),
                                                     static_pass_id_vec.end  ());
    const std::set<uint64_t> unaffected_draw_key_set(in_recipe.unaffected.draw_key_vec.begin(),
                                                     in_recipe.unaffected.draw_key_vec.end  ());
    const std::set<uint64_t> unaffected_pass_key_set(in_recipe.unaffected.pass_key_vec.begin(),
                                                     in_recipe.unaffected.pass_key_vec.end  ());

    std::vector<OverlayCandidate> result_vec;
    const auto                    timeline   = Analysis::APIDumpAnalysis::TimelineBuilder::build(in_analysis);

    for (uint32_t n_pass = 0;
                  n_pass < model_ref.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        uint64_t    first_frame_index = 0;
        const bool  has_first_frame   = Analysis::APIDumpAnalysis::QueryCommands::get_first_frame_index(in_analysis,
                                                                                                        n_pass,
                                                                                                       &first_frame_index);
        const auto& structure         = model_ref.pass_structure_interner.get_structure                (n_pass);

        const auto frame_index_vec        = Analysis::APIDumpAnalysis::QueryCommands::get_frame_index_vec(in_analysis,
                                                                                                          n_pass);
        const auto sample_frame_index_vec = get_sample_frame_index_vec(frame_index_vec);

        const auto depth_spread      = Analysis::APIDumpAnalysis::QueryCommands::get_pass_depth_spread(in_analysis,
                                                                                                       n_pass);
        const bool is_static_camera  = (static_pass_id_set.count(n_pass) != 0);


        for (const auto& draw_signature : structure.draw_signature_vec)
        {
            OverlayCandidate candidate;
            const bool       is_skipped         = applier.is_draw_skipped (structure.key,
                                                                           draw_signature.structure_key);
            const bool       is_replay_affected = applier.is_draw_affected(structure.key,
                                                                           draw_signature.structure_key);

            candidate.depth_spread         = depth_spread;
            candidate.first_frame_index    = first_frame_index;
            candidate.sample_frame_index_vec = sample_frame_index_vec;
            candidate.has_first_frame      = has_first_frame;
            candidate.is_replay_skipped    = is_skipped;
            candidate.is_replay_unaffected = (is_replay_affected == false &&
                                              is_skipped         == false);

            candidate.live_class = get_live_class(applier,
                                                  in_analysis,
                                                  n_pass,
                                                  draw_signature.signature_key);

            candidate.n_frames             = timeline.pass_usage_vec.at(n_pass).n_frames;
            candidate.pass_id              = n_pass;
            candidate.pass_key             = structure.key;
            candidate.signature_key        = draw_signature.signature_key;
            candidate.structure_key        = draw_signature.structure_key;

            const bool is_recipe_overlay = (candidate.is_replay_unaffected                          ||
                                            candidate.live_class           == LiveClass::UNAFFECTED ||
                                            candidate.live_class           == LiveClass::MIXED);
            const auto structure_iterator = model_ref.draw_structure_map.find(draw_signature.structure_key);
            const bool is_orthographic   = (structure_iterator != model_ref.draw_structure_map.end() &&
                                            structure_iterator->second.is_orthographic);

            /* A camera that never changes, and a projection that is orthographic, are the hint that the analysis has: a good sign, and no more. */
            const bool is_hinted         = (is_static_camera ||
                                            is_orthographic);

            if (is_recipe_overlay ||
                is_hinted)
            {
                candidate.agreement = (is_recipe_overlay && is_hinted) ? OverlayAgreement::BOTH
                                    : (is_recipe_overlay)              ? OverlayAgreement::RECIPE_ONLY
                                                                       : OverlayAgreement::HEURISTIC_ONLY;

                if (is_static_camera)
                {
                    candidate.reason_vec.push_back("static_camera");
                }

                if (is_orthographic)
                {
                    candidate.reason_vec.push_back("orthographic_projection");
                }

                if (depth_spread.has_depth)
                {
                    candidate.reason_vec.push_back((depth_spread.is_flat) ? "flat_depth"
                                                                          : "deep_depth");
                }

                if (structure.clears_color == false)
                {
                    candidate.reason_vec.push_back("draws_over_existing_color");
                }

                /* What the draws of the structure did, which says whether the overlay has content of its own or works on the picture below it. */
                auto facts_iterator = model_ref.draw_facts_map.lower_bound(Analysis::APIDumpAnalysis::DrawFactsKey(draw_signature.structure_key,
                                                                                                                   0) );

                for (;
                     facts_iterator != model_ref.draw_facts_map.end() &&
                     facts_iterator->first.first == draw_signature.structure_key;
                   ++facts_iterator)
                {
                    candidate.draw_facts     = (candidate.has_draw_facts) ? merge_draw_facts(candidate.draw_facts,
                                                                                             facts_iterator->second)
                                                                          : facts_iterator->second;
                    candidate.has_draw_facts = true;

                    candidate.draw_group_map[facts_iterator->first.second] = facts_iterator->second;
                }

                if (candidate.has_draw_facts)
                {
                    const auto& facts = candidate.draw_facts;

                    if (facts.n_draws_sampling_copied_color != 0)
                    {
                        candidate.reason_vec.push_back("samples_copied_picture");
                    }

                    if (facts.n_draws != 0 &&
                        facts.n_blended_draws == facts.n_draws)
                    {
                        candidate.reason_vec.push_back("blends_over_picture");
                    }

                    if (facts.n_draws_with_footprint != 0 &&
                        facts.area_fraction_min      >= COVERS_VIEW_FRACTION)
                    {
                        candidate.reason_vec.push_back("covers_view");
                    }
                    else
                    if (facts.n_draws_with_footprint != 0 &&
                        facts.area_fraction_max      >= COVERS_VIEW_FRACTION)
                    {
                        candidate.reason_vec.push_back("covers_view_in_some_draws");
                    }
                }

                if (unaffected_draw_key_set.count(draw_signature.structure_key) != 0)
                {
                    candidate.reason_vec.push_back("recipe_unaffected_draw");
                }

                if (unaffected_pass_key_set.count(structure.key) != 0)
                {
                    candidate.reason_vec.push_back("recipe_unaffected_pass");
                }

                const auto is_unaffected_func = [&](const Analysis::Recipe::PassDraw& in_pass_draw)
                {
                    return (in_pass_draw.pass_key == structure.key                &&
                            in_pass_draw.draw_key == draw_signature.structure_key);
                };

                if (std::any_of(in_recipe.unaffected.pass_draw_vec.begin(),
                                in_recipe.unaffected.pass_draw_vec.end  (),
                                is_unaffected_func) )
                {
                    candidate.reason_vec.push_back("recipe_unaffected_draw_in_pass");
                }

                if (candidate.live_class == LiveClass::UNAFFECTED)
                {
                    candidate.reason_vec.push_back("recipe_rule_unaffected");
                }

                if (candidate.live_class == LiveClass::MIXED)
                {
                    candidate.reason_vec.push_back("recipe_rule_unaffected_for_some_cameras");
                }

                if (is_skipped)
                {
                    candidate.reason_vec.push_back("recipe_skips_draw");
                }

                /* The replay and the converter that plays the application as it runs ask the recipe differently: they should come to the same. */
                if (candidate.is_replay_unaffected != (candidate.live_class == LiveClass::UNAFFECTED) )
                {
                    candidate.reason_vec.push_back("replay_and_live_disagree");
                }

                result_vec.push_back(candidate);
            }
        }
    }

    return result_vec;
}

const char* Analysis::RecipeBake::OverlayCandidateFinder::get_agreement_name(const OverlayAgreement& in_agreement)
{
    const char* result_ptr = "unknown";

    switch (in_agreement)
    {
        case OverlayAgreement::BOTH:
        {
            result_ptr = "both";

            break;
        }

        case OverlayAgreement::HEURISTIC_ONLY:
        {
            result_ptr = "heuristic_only";

            break;
        }

        case OverlayAgreement::RECIPE_ONLY:
        {
            result_ptr = "recipe_only";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_agreement);

            break;
        }
    }

    return result_ptr;
}

Analysis::RecipeBake::LiveClass Analysis::RecipeBake::OverlayCandidateFinder::get_live_class(const Analysis::Recipe::RecipeApplier&           in_applier,
                                                                                             const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                                                             const uint32_t&                                  in_pass_id,
                                                                                             const uint64_t&                                  in_signature_key)
{
    constexpr size_t N_MATRIX_BYTES = 16 * sizeof(float);

    const auto&                          model_ref = in_analysis.model;
    std::vector<std::array<float, 16> >  camera_vec;
    uint32_t    n_affected   = 0;
    uint32_t    n_skipped    = 0;
    uint32_t    n_unaffected = 0;
    const auto& structure    = model_ref.pass_structure_interner.get_structure(in_pass_id);

    for (size_t n_constant = 0;
                n_constant < structure.constant_signature_vec.size();
              ++n_constant)
    {
        const auto series_iter = model_ref.constant_series_map.find(Analysis::APIDumpAnalysis::ConstantSeriesKey(in_pass_id,
                                                                                                                 static_cast<uint32_t>(n_constant) ));

        if (structure.constant_signature_vec.at(n_constant).semantic == Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX &&
            series_iter                                              != model_ref.constant_series_map.end() )
        {
            for (const auto& sample : series_iter->second.get_sample_vec() )
            {
                if (sample.value_u8_vec.size() == N_MATRIX_BYTES)
                {
                    std::array<float, 16> matrix;

                    ::memcpy(matrix.data(),
                             sample.value_u8_vec.data(),
                             N_MATRIX_BYTES);

                    camera_vec.push_back(matrix);
                }
            }
        }
    }

    /* A pass with no camera is asked once, with numbers that no rule is about. */
    if (camera_vec.empty() )
    {
        camera_vec.push_back(std::array<float, 16>() );
    }

    for (const auto& camera : camera_vec)
    {
        switch (in_applier.classify_draw(in_signature_key,
                                         camera) )
        {
            case Analysis::Recipe::DrawClass::AFFECTED:
            {
                n_affected++;

                break;
            }

            case Analysis::Recipe::DrawClass::SKIPPED:
            {
                n_skipped++;

                break;
            }

            case Analysis::Recipe::DrawClass::UNAFFECTED:
            {
                n_unaffected++;

                break;
            }

            default:
            {
                AI_FAIL_UNSUPPORTED(in_applier.classify_draw(in_signature_key,
                                                             camera) );

                break;
            }
        }
    }

    return (n_unaffected == camera_vec.size() ) ? LiveClass::UNAFFECTED
         : (n_skipped    == camera_vec.size() ) ? LiveClass::SKIPPED
         : (n_affected   == camera_vec.size() ) ? LiveClass::AFFECTED
         : (n_unaffected != 0)                  ? LiveClass::MIXED
                                                : LiveClass::AFFECTED;
}

const char* Analysis::RecipeBake::OverlayCandidateFinder::get_live_class_name(const LiveClass& in_live_class)
{
    const char* result_ptr = "unknown";

    switch (in_live_class)
    {
        case LiveClass::AFFECTED:
        {
            result_ptr = "affected";

            break;
        }

        case LiveClass::MIXED:
        {
            result_ptr = "mixed";

            break;
        }

        case LiveClass::SKIPPED:
        {
            result_ptr = "skipped";

            break;
        }

        case LiveClass::UNAFFECTED:
        {
            result_ptr = "unaffected";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_live_class);

            break;
        }
    }

    return result_ptr;
}

std::vector<uint64_t> Analysis::RecipeBake::OverlayCandidateFinder::get_sample_frame_index_vec(const std::vector<uint64_t>& in_frame_index_vec)
{
    std::vector<uint64_t> result_vec;

    if (in_frame_index_vec.empty() == false)
    {
        result_vec.push_back(in_frame_index_vec.front() );

        const uint64_t middle_frame = in_frame_index_vec.at(in_frame_index_vec.size() / 2);
        const uint64_t last_frame   = in_frame_index_vec.back();

        if (middle_frame != result_vec.back() )
        {
            result_vec.push_back(middle_frame);
        }

        if (last_frame != result_vec.back() )
        {
            result_vec.push_back(last_frame);
        }
    }

    return result_vec;
}

std::map<uint64_t, Analysis::APIDumpAnalysis::DrawFacts> Analysis::RecipeBake::OverlayCandidateFinder::get_facts_by_cover_key(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    std::map<uint64_t, Analysis::APIDumpAnalysis::DrawFacts> result;

    for (const auto& facts_pair : in_analysis.model.draw_facts_map)
    {
        const uint64_t cover_key = facts_pair.first.second;
        const auto     found     = result.find(cover_key);

        if (found == result.end() )
        {
            result.emplace(cover_key,
                           facts_pair.second);
        }
        else
        {
            found->second = merge_draw_facts(found->second,
                                             facts_pair.second);
        }
    }

    return result;
}

Analysis::APIDumpAnalysis::DrawFacts Analysis::RecipeBake::OverlayCandidateFinder::merge_draw_facts(const Analysis::APIDumpAnalysis::DrawFacts& in_a,
                                                                                                   const Analysis::APIDumpAnalysis::DrawFacts& in_b)
{
    Analysis::APIDumpAnalysis::DrawFacts result = in_a;

    result.n_blended_draws               += in_b.n_blended_draws;
    result.n_draws                       += in_b.n_draws;
    result.n_draws_after_3d_draw         += in_b.n_draws_after_3d_draw;
    result.n_draws_eye_space             += in_b.n_draws_eye_space;
    result.n_draws_flat_layer            += in_b.n_draws_flat_layer;
    result.n_draws_sampling_copied_color += in_b.n_draws_sampling_copied_color;

    if (in_b.n_draws_with_footprint != 0)
    {
        const bool is_first = (in_a.n_draws_with_footprint == 0);

        result.area_fraction_max = (is_first || in_b.area_fraction_max > in_a.area_fraction_max) ? in_b.area_fraction_max
                                                                                                 : in_a.area_fraction_max;
        result.area_fraction_min = (is_first || in_b.area_fraction_min < in_a.area_fraction_min) ? in_b.area_fraction_min
                                                                                                 : in_a.area_fraction_min;
        result.view_depth_max    = (is_first || in_b.view_depth_max    > in_a.view_depth_max)    ? in_b.view_depth_max
                                                                                                 : in_a.view_depth_max;
        result.view_depth_min    = (is_first || in_b.view_depth_min    < in_a.view_depth_min)    ? in_b.view_depth_min
                                                                                                 : in_a.view_depth_min;
        result.x_max             = (is_first || in_b.x_max             > in_a.x_max)             ? in_b.x_max
                                                                                                 : in_a.x_max;
        result.x_min             = (is_first || in_b.x_min             < in_a.x_min)             ? in_b.x_min
                                                                                                 : in_a.x_min;
        result.y_max             = (is_first || in_b.y_max             > in_a.y_max)             ? in_b.y_max
                                                                                                 : in_a.y_max;
        result.y_min             = (is_first || in_b.y_min             < in_a.y_min)             ? in_b.y_min
                                                                                                 : in_a.y_min;
    }

    result.n_draws_with_footprint += in_b.n_draws_with_footprint;

    return result;
}

void Analysis::RecipeBake::OverlayCandidateFinder::write_json(const std::vector<OverlayCandidate>& in_candidate_vec,
                                                              const Analysis::Recipe::Recipe&      in_recipe,
                                                              Utils::Json::JsonWriter*             inout_writer_ptr)
{
    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key   ("recipe");
    inout_writer_ptr->write_string(in_recipe.target_name);
    inout_writer_ptr->write_key   ("note");
    inout_writer_ptr->write_string("A candidate is a draw that the recipe leaves alone (replay: by the keys of the draw and its pass; live: by its signature, over the cameras of its pass), or "
                                   "a draw of a pass whose camera never changes, or a draw that was drawn with an orthographic projection (a good sign of a 2D overlay, and not a sure one). Where they "
                                   "disagree is where to look.");

    inout_writer_ptr->write_key("candidates");
    inout_writer_ptr->begin_array();

    for (const auto& candidate : in_candidate_vec)
    {
        inout_writer_ptr->begin_object();

        inout_writer_ptr->write_key    ("signature_key");
        inout_writer_ptr->write_hex_u64(candidate.signature_key);
        inout_writer_ptr->write_key    ("structure_key");
        inout_writer_ptr->write_hex_u64(candidate.structure_key);
        inout_writer_ptr->write_key    ("pass_id");
        inout_writer_ptr->write_u64    (candidate.pass_id);
        inout_writer_ptr->write_key    ("pass_key");
        inout_writer_ptr->write_hex_u64(candidate.pass_key);
        inout_writer_ptr->write_key    ("n_frames");
        inout_writer_ptr->write_u64    (candidate.n_frames);

        if (candidate.has_first_frame)
        {
            inout_writer_ptr->write_key("first_frame_index");
            inout_writer_ptr->write_u64(candidate.first_frame_index);
        }

        inout_writer_ptr->write_key("sample_frame_indices");
        inout_writer_ptr->begin_inline_array();

        for (const auto frame_index : candidate.sample_frame_index_vec)
        {
            inout_writer_ptr->write_u64(frame_index);
        }

        inout_writer_ptr->end_array();

        inout_writer_ptr->write_key   ("agreement");
        inout_writer_ptr->write_string(get_agreement_name(candidate.agreement) );
        inout_writer_ptr->write_key   ("replay_class");
        inout_writer_ptr->write_string((candidate.is_replay_skipped)    ? "skipped"
                                     : (candidate.is_replay_unaffected) ? "unaffected"
                                                                        : "affected");
        inout_writer_ptr->write_key   ("live_class");
        inout_writer_ptr->write_string(get_live_class_name(candidate.live_class) );

        Analysis::APIDumpAnalysis::QueryCommands::write_depth_spread(candidate.depth_spread,
                                                                      inout_writer_ptr);

        if (candidate.has_draw_facts)
        {
            Analysis::APIDumpAnalysis::QueryCommands::write_draw_facts(candidate.draw_facts,
                                                                        inout_writer_ptr);

            /* A structure whose draws are not all alike says so by the size of its draws; the report does not carry this. */
            inout_writer_ptr->write_key("draw_groups");
            inout_writer_ptr->begin_array();

            for (const auto& group_pair : candidate.draw_group_map)
            {
                inout_writer_ptr->begin_object();
                inout_writer_ptr->write_key    ("cover_key");
                inout_writer_ptr->write_hex_u64(group_pair.first);

                Analysis::APIDumpAnalysis::QueryCommands::write_draw_facts(group_pair.second,
                                                                            inout_writer_ptr);

                inout_writer_ptr->end_object();
            }

            inout_writer_ptr->end_array();
        }

        inout_writer_ptr->write_key("reasons");
        inout_writer_ptr->begin_inline_array();

        for (const auto& reason : candidate.reason_vec)
        {
            inout_writer_ptr->write_string(reason);
        }

        inout_writer_ptr->end_array();

        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->end_object();
}

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/query_commands.h"
#include "Analysis/APIDumpAnalysis/timeline_builder.h"
#include "Analysis/RecipeBake/overlay_candidate_finder.h"
#include "Common/macros.h"
#include <algorithm>
#include <array>
#include <set>
#include <string.h>

std::vector<Analysis::RecipeBake::OverlayCandidate> Analysis::RecipeBake::OverlayCandidateFinder::find(const Analysis::RecipeLoader::Recipe&            in_recipe,
                                                                                                         const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    const Analysis::RecipeApplier::RecipeApplier applier        (in_recipe);
    const auto&                                  model_ref      = in_analysis.model;
    const std::vector<uint32_t>                  static_pass_id_vec = Analysis::APIDumpAnalysis::QueryCommands::get_static_camera_pass_id_vec(in_analysis);
    const std::set<uint32_t>                     static_pass_id_set (static_pass_id_vec.begin(),
                                                                     static_pass_id_vec.end  () );
    const std::set<uint64_t>                     unaffected_draw_key_set(in_recipe.unaffected.draw_key_vec.begin(),
                                                                         in_recipe.unaffected.draw_key_vec.end  () );
    const std::set<uint64_t>                     unaffected_pass_key_set(in_recipe.unaffected.pass_key_vec.begin(),
                                                                         in_recipe.unaffected.pass_key_vec.end  () );
    const auto                                   timeline       = Analysis::APIDumpAnalysis::TimelineBuilder::build(in_analysis);
    std::vector<OverlayCandidate>                result_vec;

    for (uint32_t n_pass = 0;
                  n_pass < model_ref.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        const auto& structure         = model_ref.pass_structure_interner.get_structure(n_pass);
        uint64_t    first_frame_index = 0;
        const bool  has_first_frame   = Analysis::APIDumpAnalysis::QueryCommands::get_first_frame_index(in_analysis,
                                                                                                        n_pass,
                                                                                                       &first_frame_index);
        const bool  is_static_camera  = (static_pass_id_set.count(n_pass) != 0);
        const auto  depth_spread      = Analysis::APIDumpAnalysis::QueryCommands::get_pass_depth_spread(in_analysis,
                                                                                                         n_pass);

        for (const auto& draw_signature : structure.draw_signature_vec)
        {
            OverlayCandidate candidate;
            const bool       is_skipped         = applier.is_draw_skipped (structure.key,
                                                                           draw_signature.structure_key);
            const bool       is_replay_affected = applier.is_draw_affected(structure.key,
                                                                           draw_signature.structure_key);

            candidate.depth_spread         = depth_spread;
            candidate.first_frame_index    = first_frame_index;
            candidate.has_first_frame      = has_first_frame;
            candidate.is_replay_skipped    = is_skipped;
            candidate.is_replay_unaffected = (is_replay_affected == false &&
                                              is_skipped         == false);
            candidate.live_class           = get_live_class(applier,
                                                            in_analysis,
                                                            n_pass,
                                                            draw_signature.signature_key);
            candidate.n_frames             = timeline.pass_usage_vec.at(n_pass).n_frames;
            candidate.pass_id              = n_pass;
            candidate.pass_key             = structure.key;
            candidate.signature_key        = draw_signature.signature_key;
            candidate.structure_key        = draw_signature.structure_key;

            const bool is_recipe_overlay = (candidate.is_replay_unaffected                ||
                                            candidate.live_class == LiveClass::UNAFFECTED ||
                                            candidate.live_class == LiveClass::MIXED);

            if (is_recipe_overlay ||
                is_static_camera)
            {
                candidate.agreement = (is_recipe_overlay && is_static_camera) ? OverlayAgreement::BOTH
                                    : (is_recipe_overlay)                     ? OverlayAgreement::RECIPE_ONLY
                                                                              : OverlayAgreement::HEURISTIC_ONLY;

                if (is_static_camera)
                {
                    candidate.reason_vec.push_back("static_camera");
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

                if (unaffected_draw_key_set.count(draw_signature.structure_key) != 0)
                {
                    candidate.reason_vec.push_back("recipe_unaffected_draw");
                }

                if (unaffected_pass_key_set.count(structure.key) != 0)
                {
                    candidate.reason_vec.push_back("recipe_unaffected_pass");
                }

                if (std::any_of(in_recipe.unaffected.pass_draw_vec.begin(),
                                in_recipe.unaffected.pass_draw_vec.end  (),
                                [&](const Analysis::RecipeLoader::PassDraw& in_pass_draw)
                                {
                                    return (in_pass_draw.pass_key == structure.key &&
                                            in_pass_draw.draw_key == draw_signature.structure_key);
                                }) )
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

Analysis::RecipeBake::LiveClass Analysis::RecipeBake::OverlayCandidateFinder::get_live_class(const Analysis::RecipeApplier::RecipeApplier&    in_applier,
                                                                                             const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                                                             const uint32_t&                                  in_pass_id,
                                                                                             const uint64_t&                                  in_signature_key)
{
    constexpr size_t N_MATRIX_BYTES = 16 * sizeof(float);

    const auto&                          model_ref = in_analysis.model;
    std::vector<std::array<float, 16> >  camera_vec;
    uint32_t                             n_affected   = 0;
    uint32_t                             n_skipped    = 0;
    uint32_t                             n_unaffected = 0;
    const auto&                          structure = model_ref.pass_structure_interner.get_structure(in_pass_id);

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
            case Analysis::RecipeLoader::DrawClass::AFFECTED:
            {
                n_affected++;

                break;
            }

            case Analysis::RecipeLoader::DrawClass::SKIPPED:
            {
                n_skipped++;

                break;
            }

            case Analysis::RecipeLoader::DrawClass::UNAFFECTED:
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

void Analysis::RecipeBake::OverlayCandidateFinder::write_json(const std::vector<OverlayCandidate>&   in_candidate_vec,
                                                              const Analysis::RecipeLoader::Recipe&  in_recipe,
                                                              Analysis::APIDumpAnalysis::JsonWriter* inout_writer_ptr)
{
    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key   ("recipe");
    inout_writer_ptr->write_string(in_recipe.target_name);
    inout_writer_ptr->write_key   ("note");
    inout_writer_ptr->write_string("A candidate is a draw that the recipe leaves alone (replay: by the keys of the draw and its pass; live: by its signature, over the cameras of its pass), or "
                                   "a draw of a pass whose camera never changes. Where they disagree is where to look.");

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

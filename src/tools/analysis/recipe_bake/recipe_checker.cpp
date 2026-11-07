/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/model_builder.h"
#include "overlay_candidate_finder.h"
#include "recipe_checker.h"
#include <algorithm>
#include <map>
#include <set>
#include <string>

Analysis::RecipeBake::CoverageReport Analysis::RecipeBake::RecipeChecker::check(const Analysis::Recipe::Recipe&                  in_recipe,
                                                                                const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    const auto& model_ref = in_analysis.model;

    const std::set<uint64_t> known_shape_set    (in_recipe.known_shape_key_vec.begin    (),
                                                 in_recipe.known_shape_key_vec.end      ());
    const std::set<uint64_t> known_signature_set(in_recipe.known_signature_key_vec.begin(),
                                                 in_recipe.known_signature_key_vec.end  ());

    CoverageReport                   result;
    std::map<uint32_t, UnknownShape> shape_map;
    std::map<uint64_t, uint64_t>     signature_pass_kind_count_map;

    for (const auto& frame_record : model_ref.frame_record_vec)
    {
        UnknownShape& shape = shape_map[frame_record.shape_id];

        if (shape.n_frames == 0)
        {
            shape.first_frame_index = frame_record.frame_index;
        }

        shape.n_frames++;
    }

    result.n_frames = model_ref.frame_record_vec.size();
    result.n_shapes = shape_map.size();

    for (auto& shape_pair : shape_map)
    {
        shape_pair.second.shape_key = Analysis::APIDumpAnalysis::ModelBuilder::compute_shape_key(model_ref,
                                                                                                 shape_pair.first);

        const bool is_known = (known_shape_set.empty()                                 ||
                               known_shape_set.count(shape_pair.second.shape_key) != 0);

        if (is_known == false)
        {
            result.n_frames_of_unknown_shapes += shape_pair.second.n_frames;

            result.unknown_shape_vec.push_back(shape_pair.second);
        }
    }

    {
        const auto is_shape_less_func = [](const UnknownShape& in_shape_a,
                                           const UnknownShape& in_shape_b)
        {
            return in_shape_a.first_frame_index < in_shape_b.first_frame_index;
        };

        std::sort(result.unknown_shape_vec.begin(),
                  result.unknown_shape_vec.end  (),
                  is_shape_less_func);
    }

    for (uint32_t n_pass = 0;
                  n_pass < model_ref.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        std::set<uint64_t> signature_key_set;

        for (const auto& draw_signature : model_ref.pass_structure_interner.get_structure(n_pass).draw_signature_vec)
        {
            signature_key_set.insert(draw_signature.signature_key);
        }

        for (const auto signature_key : signature_key_set)
        {
            signature_pass_kind_count_map[signature_key]++;
        }
    }

    result.n_signatures = signature_pass_kind_count_map.size();

    /* What the recipe leaves alone as the overlays are, a pass at a time: the first draw that tells it is the one that is reported. */
    std::set<uint32_t> disagreeing_pass_set;
    std::set<uint32_t> deep_pass_set;

    for (const auto& candidate : OverlayCandidateFinder::find(in_recipe,
                                                              in_analysis) )
    {
        const bool is_left_alone  = (candidate.is_replay_unaffected                            ||
                                     candidate.live_class             == LiveClass::UNAFFECTED ||
                                     candidate.live_class             == LiveClass::MIXED);
        const bool is_deep        = (is_left_alone                                             &&
                                     candidate.depth_spread.has_depth                          &&
                                     candidate.depth_spread.is_flat   == false);

        const bool is_disagreeing = (std::find(candidate.reason_vec.begin(),
                                               candidate.reason_vec.end  (),
                                               "replay_and_live_disagree") != candidate.reason_vec.end() );

        if (is_deep)
        {
            if (deep_pass_set.insert(candidate.pass_id).second)
            {
                result.unaffected_warning_vec.push_back(make_warning(candidate,
                                                                     UnaffectedWarningKind::DEEP_PASS) );
            }
        }

        if (is_disagreeing)
        {
            if (disagreeing_pass_set.insert(candidate.pass_id).second)
            {
                result.unaffected_warning_vec.push_back(make_warning(candidate,
                                                                     UnaffectedWarningKind::REPLAY_AND_LIVE_DISAGREE) );
            }
        }
    }

    for (const auto& signature_pair : signature_pass_kind_count_map)
    {
        const bool is_known = (known_signature_set.empty()                           ||
                               known_signature_set.count(signature_pair.first) != 0);

        if (is_known == false)
        {
            UnknownSignature signature;

            signature.n_pass_structures = signature_pair.second;
            signature.signature_key     = signature_pair.first;

            result.unknown_signature_vec.push_back(signature);
        }
    }

    return result;
}

Analysis::RecipeBake::UnaffectedWarning Analysis::RecipeBake::RecipeChecker::make_warning(const OverlayCandidate&      in_candidate,
                                                                                          const UnaffectedWarningKind& in_kind)
{
    UnaffectedWarning result;

    result.depth_spread      = in_candidate.depth_spread;
    result.first_frame_index = in_candidate.first_frame_index;
    result.kind              = in_kind;
    result.n_frames          = in_candidate.n_frames;
    result.pass_id           = in_candidate.pass_id;
    result.pass_key          = in_candidate.pass_key;

    return result;
}

void Analysis::RecipeBake::RecipeChecker::print_report(FILE*                           in_file_handle,
                                                       const CoverageReport&           in_report,
                                                       const Analysis::Recipe::Recipe& in_recipe)
{
    constexpr size_t N_ENTRIES_LISTED = 40;

    std::string policy_text;

    switch (in_recipe.unknown_frame_policy)
    {
        case Analysis::Recipe::UnknownFramePolicy::FAIL:
        {
            policy_text = "stops the replay";

            break;
        }

        case Analysis::Recipe::UnknownFramePolicy::MONO:
        {
            policy_text = "is played as it is, for both eyes";

            break;
        }

        case Analysis::Recipe::UnknownFramePolicy::SKIP:
        {
            policy_text = "is not played";

            break;
        }

        default:
        {
            policy_text = "is played as the recipe says";

            break;
        }
    }

    fprintf(in_file_handle,
            "The workload has %llu frames of %llu kinds, and %llu draw signatures.\n",
            static_cast<unsigned long long>(in_report.n_frames),
            static_cast<unsigned long long>(in_report.n_shapes),
            static_cast<unsigned long long>(in_report.n_signatures) );

    if (in_report.unknown_shape_vec.empty() )
    {
        fprintf(in_file_handle,
                "The recipe knows every kind of frame.\n");
    }
    else
    {
        fprintf(in_file_handle,
                "%zu kinds of frames, %llu frames, are not known to the recipe; a frame of them %s:\n",
                in_report.unknown_shape_vec.size(),
                static_cast<unsigned long long>(in_report.n_frames_of_unknown_shapes),
                policy_text.c_str() );

        for (size_t n_shape = 0;
                    n_shape < in_report.unknown_shape_vec.size() && n_shape < N_ENTRIES_LISTED;
                  ++n_shape)
        {
            const auto& shape = in_report.unknown_shape_vec.at(n_shape);

            fprintf(in_file_handle,
                    "  shape 0x%016llx: %llu frames, the first is frame %llu\n",
                    static_cast<unsigned long long>(shape.shape_key),
                    static_cast<unsigned long long>(shape.n_frames),
                    static_cast<unsigned long long>(shape.first_frame_index) );
        }
    }

    if (in_report.unknown_signature_vec.empty() )
    {
        fprintf(in_file_handle,
                "The recipe knows every draw signature.\n");
    }
    else
    {
        fprintf(in_file_handle,
                "%zu draw signatures are not known to the recipe; a draw of them is an affected one:\n",
                in_report.unknown_signature_vec.size() );

        for (size_t n_signature = 0;
                    n_signature < in_report.unknown_signature_vec.size() && n_signature < N_ENTRIES_LISTED;
                  ++n_signature)
        {
            const auto& signature = in_report.unknown_signature_vec.at(n_signature);

            fprintf(in_file_handle,
                    "  signature 0x%016llx: drawn by %llu kinds of passes\n",
                    static_cast<unsigned long long>(signature.signature_key),
                    static_cast<unsigned long long>(signature.n_pass_structures) );
        }
    }

    if (in_report.unaffected_warning_vec.empty() )
    {
        fprintf(in_file_handle,
                "What the recipe leaves alone looks like overlays, and a replay and the converter treat it alike.\n");
    }
    else
    {
        fprintf(in_file_handle,
                "%zu things about what the recipe leaves alone (see the overlay probe, which shows what the draws make):\n",
                in_report.unaffected_warning_vec.size() );

        for (size_t n_warning = 0;
                    n_warning < in_report.unaffected_warning_vec.size() && n_warning < N_ENTRIES_LISTED;
                  ++n_warning)
        {
            const auto& warning = in_report.unaffected_warning_vec.at(n_warning);

            if (warning.kind == UnaffectedWarningKind::DEEP_PASS)
            {
                fprintf(in_file_handle,
                        "  pass 0x%016llx (%llu frames, the first is frame %llu) is left alone, and its vertices are from depth %.1f to %.1f (%.1f times): a 3D scene seen from one place, "
                        "which would be pushed as if it were flat\n",
                        static_cast<unsigned long long>(warning.pass_key),
                        static_cast<unsigned long long>(warning.n_frames),
                        static_cast<unsigned long long>(warning.first_frame_index),
                        warning.depth_spread.depth_near,
                        warning.depth_spread.depth_far,
                        warning.depth_spread.far_to_near_ratio);
            }
            else
            {
                fprintf(in_file_handle,
                        "  pass 0x%016llx (%llu frames, the first is frame %llu) is left alone by a replay and not by the converter that plays the application as it runs "
                        "(it knows no passes: name draws, not passes)\n",
                        static_cast<unsigned long long>(warning.pass_key),
                        static_cast<unsigned long long>(warning.n_frames),
                        static_cast<unsigned long long>(warning.first_frame_index) );
            }
        }
    }
}

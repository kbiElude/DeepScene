/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/model_builder.h"
#include "Analysis/RecipeBake/recipe_checker.h"
#include <algorithm>
#include <map>
#include <set>
#include <string>

Analysis::RecipeBake::CoverageReport Analysis::RecipeBake::RecipeChecker::check(const Analysis::RecipeLoader::Recipe&            in_recipe,
                                                                                const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    const auto&                           model_ref            = in_analysis.model;
    const std::set<uint64_t>              known_shape_set      (in_recipe.known_shape_key_vec.begin(),
                                                                in_recipe.known_shape_key_vec.end() );
    const std::set<uint64_t>              known_signature_set  (in_recipe.known_signature_key_vec.begin(),
                                                                in_recipe.known_signature_key_vec.end() );
    CoverageReport                        result;
    std::map<uint32_t, UnknownShape>      shape_map;
    std::map<uint64_t, uint64_t>          signature_pass_kind_count_map;

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

        const bool is_known = (known_shape_set.empty() ||
                               known_shape_set.count(shape_pair.second.shape_key) != 0);

        if (is_known == false)
        {
            result.n_frames_of_unknown_shapes += shape_pair.second.n_frames;

            result.unknown_shape_vec.push_back(shape_pair.second);
        }
    }

    std::sort(result.unknown_shape_vec.begin(),
              result.unknown_shape_vec.end  (),
              [](const UnknownShape& in_shape_a,
                 const UnknownShape& in_shape_b)
              {
                  return in_shape_a.first_frame_index < in_shape_b.first_frame_index;
              });

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

    for (const auto& signature_pair : signature_pass_kind_count_map)
    {
        const bool is_known = (known_signature_set.empty() ||
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

void Analysis::RecipeBake::RecipeChecker::print_report(FILE*                                 in_file_handle,
                                                        const CoverageReport&                 in_report,
                                                        const Analysis::RecipeLoader::Recipe& in_recipe)
{
    constexpr size_t N_ENTRIES_LISTED = 40;

    std::string policy_text;

    switch (in_recipe.unknown_frame_policy)
    {
        case Analysis::RecipeLoader::UnknownFramePolicy::FAIL:
        {
            policy_text = "stops the replay";

            break;
        }

        case Analysis::RecipeLoader::UnknownFramePolicy::MONO:
        {
            policy_text = "is played as it is, for both eyes";

            break;
        }

        case Analysis::RecipeLoader::UnknownFramePolicy::SKIP:
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
}

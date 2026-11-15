/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the match keys: what they depend on, and what they must not depend on.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/match_key_builder.h"
#include <algorithm>
#include <functional>
#include <vector>

TEST(MatchKeyBuilder, EqualDescriptionsHaveEqualKeys)
{
    Analysis::APIDumpAnalysis::ResourceDescription a;
    Analysis::APIDumpAnalysis::ResourceDescription b;

    a.width  = 512;
    a.height = 512;
    b        = a;

    EXPECT_EQ(Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_description_key(a),
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_description_key(b) );
}

TEST(MatchKeyBuilder, EveryFieldOfADescriptionChangesItsKey)
{
    typedef std::function<void(Analysis::APIDumpAnalysis::ResourceDescription*)> MutationFunc;

    const std::vector<MutationFunc> mutation_func_vec =
    {
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->api_flags = 1;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->content_hash = 1;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->depth_or_layer_count = 2;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->format.format_class = Analysis::APIDumpAnalysis::FormatClass::DEPTH;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->format.raw_value = 22;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->has_content_hash = true;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->height = 7;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->is_depth_stencil = true;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->is_dynamic = true;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->is_render_target = true;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->kind = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->mip_level_count = 9;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->sample_count = 4;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->shader_stage = Analysis::APIDumpAnalysis::ShaderStage::VERTEX;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->size_in_bytes = 64;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->is_application_memory = true;
        },
        [](Analysis::APIDumpAnalysis::ResourceDescription* description_ptr)
        {
            description_ptr->width = 7;
        }
    };

    const Analysis::APIDumpAnalysis::ResourceDescription original;
    const uint64_t                                       original_key = Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_description_key(original);
    std::vector<uint64_t>                                key_vec;

    for (size_t n_mutation = 0;
                n_mutation < mutation_func_vec.size();
              ++n_mutation)
    {
        Analysis::APIDumpAnalysis::ResourceDescription mutated = original;

        mutation_func_vec.at(n_mutation)(&mutated);

        const uint64_t key = Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_description_key(mutated);

        EXPECT_NE(key,
                  original_key) << "mutation " << n_mutation;

        key_vec.push_back(key);
    }

    /* No two of the changes give the same key either. */
    std::sort(key_vec.begin(),
              key_vec.end() );

    EXPECT_EQ(std::adjacent_find(key_vec.begin(),
                                 key_vec.end() ),
              key_vec.end() );
}

TEST(MatchKeyBuilder, ADataBufferOfTheApplicationsMemoryIsToldApartByItsKindOnly)
{
    Analysis::APIDumpAnalysis::ResourceDescription first;
    Analysis::APIDumpAnalysis::ResourceDescription second;
    Analysis::APIDumpAnalysis::ResourceDescription index;

    first.content_hash          = 1;
    first.has_content_hash      = true;
    first.is_application_memory = true;
    first.kind                  = Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX;
    first.size_in_bytes         = 36;

    second               = first;
    second.content_hash  = 2;
    second.size_in_bytes = 72;

    index      = first;
    index.kind = Analysis::APIDumpAnalysis::ResourceKind::BUFFER_INDEX;

    EXPECT_EQ(Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_description_key(first),
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_description_key(second) );
    EXPECT_NE(Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_description_key(first),
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_description_key(index) );
}

TEST(MatchKeyBuilder, KeySetsHaveNoOrder)
{
    EXPECT_EQ(Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_key_set_key({1, 2, 3}),
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_key_set_key({3, 1, 2}) );
}

TEST(MatchKeyBuilder, KeySetsAreToldApartByTheirMembersAndTheirSize)
{
    EXPECT_NE(Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_key_set_key({1, 2, 3}),
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_key_set_key({1, 2, 4}) );
    EXPECT_NE(Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_key_set_key({1, 2}),
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_key_set_key({1, 2, 2}) );
    EXPECT_NE(Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_key_set_key({}),
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_key_set_key({0}) );
}

TEST(MatchKeyBuilder, ADrawSignatureIsToldApartByItsStructureAndItsTargets)
{
    const std::vector<uint64_t> one_color  = {1};
    const std::vector<uint64_t> two_colors = {1, 2};

    const uint64_t base = Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_draw_signature_key(10,
                                                                                                 one_color,
                                                                                                 20);

    EXPECT_EQ(base,
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_draw_signature_key(10,
                                                                                     one_color,
                                                                                     20) );
    EXPECT_NE(base,
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_draw_signature_key(11,
                                                                                     one_color,
                                                                                     20) );
    EXPECT_NE(base,
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_draw_signature_key(10,
                                                                                     two_colors,
                                                                                     20) );
    EXPECT_NE(base,
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_draw_signature_key(10,
                                                                                     one_color,
                                                                                     21) );
}

TEST(MatchKeyBuilder, ADrawStructureThatIsOrthographicHasAnotherKeyAndOthersKeepTheirs)
{
    Analysis::APIDumpAnalysis::DrawStructure structure;
    Analysis::APIDumpAnalysis::DrawStructure orthographic = structure;
    Analysis::APIDumpAnalysis::DrawStructure not_orthographic = structure;

    orthographic.is_orthographic     = true;
    not_orthographic.is_orthographic = false;

    EXPECT_NE(Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_draw_structure_key(structure),
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_draw_structure_key(orthographic) );

    /* A structure that is not orthographic has nothing added to its key, so that the recipes that were baked keep the keys they know. */
    EXPECT_EQ(Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_draw_structure_key(structure),
              Analysis::APIDumpAnalysis::MatchKeyBuilder::compute_draw_structure_key(not_orthographic) );
}

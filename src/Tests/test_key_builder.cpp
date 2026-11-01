/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the match keys: what they depend on, and what they must not depend on.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/key_builder.h"
#include <algorithm>
#include <functional>
#include <vector>

TEST(KeyBuilder, EqualDescriptionsHaveEqualKeys)
{
    Analysis::APIDumpAnalysis::ResourceDescription a;
    Analysis::APIDumpAnalysis::ResourceDescription b;

    a.width  = 512;
    a.height = 512;
    b        = a;

    EXPECT_EQ(Analysis::APIDumpAnalysis::KeyBuilder::compute_description_key(a),
              Analysis::APIDumpAnalysis::KeyBuilder::compute_description_key(b) );
}

TEST(KeyBuilder, EveryFieldOfADescriptionChangesItsKey)
{
    typedef std::function<void(Analysis::APIDumpAnalysis::ResourceDescription*)> MutationFunc;

    const std::vector<MutationFunc> mutation_func_vec =
    {
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->api_flags            = 1; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->content_hash         = 1; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->depth_or_layer_count = 2; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->format.format_class  = Analysis::APIDumpAnalysis::FormatClass::DEPTH; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->format.raw_value     = 22; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->has_content_hash     = true; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->height               = 7; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->is_depth_stencil     = true; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->is_dynamic           = true; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->is_render_target     = true; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->kind                 = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->mip_level_count      = 9; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->sample_count         = 4; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->shader_stage         = Analysis::APIDumpAnalysis::ShaderStage::VERTEX; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->size_in_bytes        = 64; },
        [](Analysis::APIDumpAnalysis::ResourceDescription* p) { p->width                = 7; }
    };

    const Analysis::APIDumpAnalysis::ResourceDescription original;
    const uint64_t                                       original_key = Analysis::APIDumpAnalysis::KeyBuilder::compute_description_key(original);
    std::vector<uint64_t>                                key_vec;

    for (size_t n_mutation = 0;
                n_mutation < mutation_func_vec.size();
              ++n_mutation)
    {
        Analysis::APIDumpAnalysis::ResourceDescription mutated = original;

        mutation_func_vec.at(n_mutation)(&mutated);

        const uint64_t key = Analysis::APIDumpAnalysis::KeyBuilder::compute_description_key(mutated);

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

TEST(KeyBuilder, KeySetsHaveNoOrder)
{
    EXPECT_EQ(Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key({1, 2, 3}),
              Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key({3, 1, 2}) );
}

TEST(KeyBuilder, KeySetsAreToldApartByTheirMembersAndTheirSize)
{
    EXPECT_NE(Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key({1, 2, 3}),
              Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key({1, 2, 4}) );
    EXPECT_NE(Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key({1, 2}),
              Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key({1, 2, 2}) );
    EXPECT_NE(Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key({}),
              Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key({0}) );
}

TEST(KeyBuilder, ADrawSignatureIsToldApartByItsStructureAndItsTargets)
{
    const std::vector<uint64_t> one_color  = {1};
    const std::vector<uint64_t> two_colors = {1, 2};

    const uint64_t base = Analysis::APIDumpAnalysis::KeyBuilder::compute_draw_signature_key(10,
                                                                                            one_color,
                                                                                            20);

    EXPECT_EQ(base,
              Analysis::APIDumpAnalysis::KeyBuilder::compute_draw_signature_key(10,
                                                                                one_color,
                                                                                20) );
    EXPECT_NE(base,
              Analysis::APIDumpAnalysis::KeyBuilder::compute_draw_signature_key(11,
                                                                                one_color,
                                                                                20) );
    EXPECT_NE(base,
              Analysis::APIDumpAnalysis::KeyBuilder::compute_draw_signature_key(10,
                                                                                two_colors,
                                                                                20) );
    EXPECT_NE(base,
              Analysis::APIDumpAnalysis::KeyBuilder::compute_draw_signature_key(10,
                                                                                one_color,
                                                                                21) );
}

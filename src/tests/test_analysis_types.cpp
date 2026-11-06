/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the vocabulary of the analysis: defaults and comparisons.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/types.h"

TEST(AnalysisTypes, ResourceIdsRoundTrip)
{
    EXPECT_EQ(Analysis::APIDumpAnalysis::get_resource_id_value(Analysis::APIDumpAnalysis::ResourceId::NONE),
              0u);
    EXPECT_EQ(Analysis::APIDumpAnalysis::get_resource_id_value(Analysis::APIDumpAnalysis::make_resource_id(42) ),
              42u);
    EXPECT_EQ(Analysis::APIDumpAnalysis::make_resource_id(7),
              Analysis::APIDumpAnalysis::make_resource_id(7) );
    EXPECT_NE(Analysis::APIDumpAnalysis::make_resource_id(7),
              Analysis::APIDumpAnalysis::ResourceId::NONE);
}

TEST(AnalysisTypes, DefaultsAreNeutral)
{
    const Analysis::APIDumpAnalysis::DrawDescription     draw;
    const Analysis::APIDumpAnalysis::Resource            resource;
    const Analysis::APIDumpAnalysis::ResourceDescription description;
    const Analysis::APIDumpAnalysis::Viewport            viewport;

    EXPECT_EQ(draw.instance_count,
              1u);
    EXPECT_EQ(draw.primitive_topology,
              Analysis::APIDumpAnalysis::PrimitiveTopology::UNKNOWN);
    EXPECT_TRUE(draw.shader_id_vec.empty() );

    EXPECT_EQ(resource.id,
              Analysis::APIDumpAnalysis::ResourceId::NONE);
    EXPECT_EQ(resource.parent_view.resource_id,
              Analysis::APIDumpAnalysis::ResourceId::NONE);
    EXPECT_FALSE(resource.is_destroyed);

    EXPECT_EQ(description.kind,
              Analysis::APIDumpAnalysis::ResourceKind::UNKNOWN);
    EXPECT_EQ(description.mip_level_count,
              1u);
    EXPECT_EQ(description.format.format_class,
              Analysis::APIDumpAnalysis::FormatClass::UNKNOWN);

    EXPECT_EQ(viewport.max_depth,
              1.0f);
}

TEST(AnalysisTypes, ClearOperationsAreComparedFieldByField)
{
    Analysis::APIDumpAnalysis::ClearOperation a;
    Analysis::APIDumpAnalysis::ClearOperation b;

    EXPECT_TRUE(a == b);

    b.color_fp32vec4[2] = 0.5f;
    EXPECT_FALSE(a == b);

    b                 = a;
    b.clears_stencil = true;
    EXPECT_FALSE(a == b);
}

TEST(AnalysisTypes, ConstantUpdatesAreComparedByValue)
{
    Analysis::APIDumpAnalysis::ConstantUpdate a;
    Analysis::APIDumpAnalysis::ConstantUpdate b;

    a.value_u8_vec = {1, 2, 3};
    b.value_u8_vec = {1, 2, 3};
    EXPECT_TRUE(a == b);

    b.value_u8_vec[2] = 4;
    EXPECT_FALSE(a == b);

    b          = a;
    b.semantic = Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX;
    EXPECT_FALSE(a == b);

    b                         = a;
    b.location.block_kind     = Analysis::APIDumpAnalysis::ConstantBlockKind::PUSH_CONSTANTS;
    EXPECT_FALSE(a == b);

    b                     = a;
    b.location.byte_offset = 64;
    EXPECT_FALSE(a == b);

    b                     = a;
    b.location.block_id   = Analysis::APIDumpAnalysis::make_resource_id(3);
    EXPECT_FALSE(a == b);

    b            = a;
    b.provenance = Analysis::APIDumpAnalysis::ConstantProvenance::INFERRED;
    EXPECT_FALSE(a == b);
}

TEST(AnalysisTypes, DrawsAreComparedIncludingTheirBindings)
{
    Analysis::APIDumpAnalysis::DrawDescription a;
    Analysis::APIDumpAnalysis::DrawDescription b;

    a.texture_binding_vec.push_back({{0, 0, 2}, {0, 1, Analysis::APIDumpAnalysis::make_resource_id(5)}});
    b.texture_binding_vec.push_back({{0, 0, 2}, {0, 1, Analysis::APIDumpAnalysis::make_resource_id(5)}});
    EXPECT_TRUE(a == b);

    b.texture_binding_vec[0].view.mip_level = 2;
    EXPECT_FALSE(a == b);

    b                                           = a;
    b.texture_binding_vec[0].binding_point.group = 1;
    EXPECT_FALSE(a == b);

    b = a;
    b.shader_id_vec.push_back(Analysis::APIDumpAnalysis::make_resource_id(9) );
    EXPECT_FALSE(a == b);

    b                      = a;
    b.vertex_buffer_id_vec = {Analysis::APIDumpAnalysis::make_resource_id(3)};
    EXPECT_FALSE(a == b);

    b                = a;
    b.state_key      = 1;
    EXPECT_FALSE(a == b);
}

TEST(AnalysisTypes, ResourceDescriptionsAreComparedFieldByField)
{
    Analysis::APIDumpAnalysis::ResourceDescription a;
    Analysis::APIDumpAnalysis::ResourceDescription b;

    a.width  = 512;
    a.height = 512;
    b        = a;
    EXPECT_TRUE(a == b);

    b.format.raw_value = 22;
    EXPECT_FALSE(a == b);

    b                = a;
    b.is_render_target = true;
    EXPECT_FALSE(a == b);

    b                  = a;
    b.has_content_hash = true;
    EXPECT_FALSE(a == b);

    b           = a;
    b.api_flags = 1;
    EXPECT_FALSE(a == b);
}

TEST(AnalysisTypes, ViewsAndViewportsAreComparedFieldByField)
{
    Analysis::APIDumpAnalysis::ResourceView view_a = {0, 0, Analysis::APIDumpAnalysis::make_resource_id(1)};
    Analysis::APIDumpAnalysis::ResourceView view_b = view_a;
    Analysis::APIDumpAnalysis::Viewport     viewport_a;
    Analysis::APIDumpAnalysis::Viewport     viewport_b;

    EXPECT_TRUE(view_a     == view_b);
    EXPECT_TRUE(viewport_a == viewport_b);

    view_b.layer = 1;
    EXPECT_FALSE(view_a == view_b);

    viewport_b.y = 45.0f;
    EXPECT_FALSE(viewport_a == viewport_b);
}

TEST(AnalysisTypes, BindingPointsAreComparedFieldByField)
{
    Analysis::APIDumpAnalysis::BindingPoint a = {1, 2, 3};
    Analysis::APIDumpAnalysis::BindingPoint b = a;

    EXPECT_TRUE(a == b);

    b.element = 0;
    EXPECT_FALSE(a == b);

    b       = a;
    b.index = 4;
    EXPECT_FALSE(a == b);
}

TEST(AnalysisTypes, DataReferencesAreComparedFieldByField)
{
    Analysis::APIDumpAnalysis::DataReference a = {0x1234, 7, 64};
    Analysis::APIDumpAnalysis::DataReference b = a;

    EXPECT_TRUE(a == b);

    b.n_bytes = 65;
    EXPECT_FALSE(a == b);
}

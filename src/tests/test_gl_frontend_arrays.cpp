/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL front end following the draws from arrays (glDrawArrays and glDrawElements): the data that such a draw reads is a
 * resource which is made once for the same bytes, whatever the draw or the frame it is read in.
 */
#include <gtest/gtest.h>
#include "gl_frontend_fixture.h"

namespace
{
    class GLFrontendArrays : public Tests::GLFrontendFixture
    {
    protected:
        typedef Tests::GLCallBuilder::ClientArray ClientArray;

        enum : uint32_t
        {
            GL_TRIANGLES = 0x00000004
        };

        /* The resources of the model of the given kind. */
        size_t count_resources(const Analysis::APIDumpAnalysis::ResourceKind& in_kind) const
        {
            size_t result = 0;

            for (const auto& resource : m_harness.get_model().resource_vec)
            {
                if (resource.description.kind == in_kind)
                {
                    ++result;
                }
            }

            return result;
        }

        /* The first resource of vertices of the model that is not the given one. */
        Analysis::APIDumpAnalysis::ResourceId get_first_vertex_resource(const Analysis::APIDumpAnalysis::ResourceId& in_other_than = Analysis::APIDumpAnalysis::ResourceId::NONE) const
        {
            Analysis::APIDumpAnalysis::ResourceId result = Analysis::APIDumpAnalysis::ResourceId::NONE;

            for (const auto& resource : m_harness.get_model().resource_vec)
            {
                if (result                    == Analysis::APIDumpAnalysis::ResourceId::NONE            &&
                    resource.description.kind == Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX &&
                    resource.id               != in_other_than)
                {
                    result = resource.id;
                }
            }

            return result;
        }

        /* A triangle whose positions are the given ones moved along x. */
        static ClientArray make_triangle(const float& in_x)
        {
            return Tests::GLCallBuilder::make_vertex_array({{in_x,        0.0f, -1.0f},
                                                            {in_x + 1.0f, 0.0f, -1.0f},
                                                            {in_x,        1.0f, -1.0f} });
        }
    };
}

TEST_F(GLFrontendArrays, ADrawOfArraysIsOneDrawWithABufferOfItsVertices)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(0.0f)});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              1u);
    EXPECT_EQ(count_resources(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX),
              1u);
    EXPECT_EQ(count_resources(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_INDEX),
              0u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unsupported_calls(),
              0u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              0u);

    ASSERT_EQ(model.draw_structure_map.size(),
              1u);
    EXPECT_FALSE(model.draw_structure_map.begin()->second.is_indexed);
}

TEST_F(GLFrontendArrays, TheSameDataIsTheSameBufferInAnyDrawOrFrame)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(0.0f)});
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(0.0f)});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);
    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(0.0f)});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(count_resources(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX),
              1u);
    EXPECT_EQ(m_harness.get_model().draw_structure_map.size(),
              1u);
}

TEST_F(GLFrontendArrays, TheBytesOfAFrameAreThereUntilItEnds)
{
    start(640,
          480);

    m_builder.clear      (COLOR_BUFFER_BIT);
    m_builder.draw_arrays(GL_TRIANGLES,
                          0,
                          3,
                          {make_triangle(0.0f)});

    feed();

    const auto& blob_store = m_harness.get_frontend().get_blob_store();
    const auto  resource   = get_first_vertex_resource();
    const auto  data_ptr   = blob_store.get_data_ptr(resource);

    /* The frame is not over: each eye can still draw it. */
    ASSERT_NE(data_ptr,
              nullptr);
    EXPECT_EQ(data_ptr->size(),
              3u * sizeof(float) * 3u);
}

TEST_F(GLFrontendArrays, DataOfOneFrameIsLetGoOfAndDataOfTwoFramesIsKept)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(0.0f)});
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(5.0f)});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);
    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(0.0f)});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& blob_store = m_harness.get_frontend().get_blob_store();
    const auto  repeated   = get_first_vertex_resource();
    const auto  once       = get_first_vertex_resource(repeated);

    EXPECT_EQ(blob_store.get_reuse(repeated),
              Analysis::APIDumpAnalysis::Frontends::GLBlobReuse::ACROSS_FRAMES);
    EXPECT_NE(blob_store.get_data_ptr(repeated),
              nullptr);
    EXPECT_EQ(blob_store.get_reuse(once),
              Analysis::APIDumpAnalysis::Frontends::GLBlobReuse::ONE_FRAME);
    EXPECT_EQ(blob_store.get_data_ptr(once),
              nullptr);
}

TEST_F(GLFrontendArrays, OtherDataIsAnotherBuffer)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(0.0f)});
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(5.0f)});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(count_resources(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX),
              2u);
}

TEST_F(GLFrontendArrays, TheBytesOfTheArraysDoNotTellDrawsApart)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(0.0f)});
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(5.0f)});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    /* Two buffers, and the draws are of one structure; the buffers are told apart by the model builder, whatever their identity in a run. */
    EXPECT_EQ(count_resources(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX),
              2u);
    EXPECT_EQ(model.draw_structure_map.size(),
              1u);
}

TEST_F(GLFrontendArrays, TheLayoutOfTheArraysTellsDrawsApart)
{
    ClientArray padded = make_triangle(0.0f);

    padded.stride = 12; /* the same data, said to be of another stride */

    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(0.0f)});
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {padded});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    EXPECT_EQ(count_resources(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX),
              1u);
    EXPECT_EQ(model.draw_structure_map.size(),
              2u);
}

TEST_F(GLFrontendArrays, ADrawOfElementsHasABufferOfItsIndices)
{
    start(640,
          480);

    m_builder.clear        (COLOR_BUFFER_BIT);
    m_builder.draw_elements(GL_TRIANGLES,
                            {0, 1, 2},
                            {make_triangle(0.0f)});
    m_builder.draw_elements(GL_TRIANGLES,
                            {0, 1, 2},
                            {make_triangle(0.0f)});
    m_builder.draw_elements(GL_TRIANGLES,
                            {2, 1, 0},
                            {make_triangle(0.0f)});
    m_builder.swap_buffers (DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              3u);
    EXPECT_EQ(count_resources(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX),
              1u);
    EXPECT_EQ(count_resources(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_INDEX),
              2u);
    /* The indices are told apart by their kind, not by which they are: the draws are of one structure. */
    EXPECT_EQ(model.draw_structure_map.size(),
              1u);

    for (const auto& draw_pair : model.draw_structure_map)
    {
        EXPECT_TRUE(draw_pair.second.is_indexed);
    }
}

TEST_F(GLFrontendArrays, ADrawOfNothingIsNoDraw)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           0,
                           {});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().frame_record_vec.at(0).n_draws,
              0u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              0u);
}

TEST_F(GLFrontendArrays, ADrawWithNoArraysInItIsADrawWithNoBuffers)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().frame_record_vec.at(0).n_draws,
              1u);
    EXPECT_EQ(count_resources(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX),
              0u);
}

TEST_F(GLFrontendArrays, ADrawMadeWhileAListIsMadeIsNotMade)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.new_list    (1,
                           0x1300);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {make_triangle(0.0f)});
    m_builder.end_list    ();
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().frame_record_vec.at(0).n_draws,
              0u);
    EXPECT_EQ(count_resources(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX),
              0u);
}

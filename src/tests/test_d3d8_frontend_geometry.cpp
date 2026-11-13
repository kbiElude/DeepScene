/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the D3D8 front end telling where the vertices of a draw are: from what was written to the buffers it draws from.
 */
#include <gtest/gtest.h>
#include "d3d8_scene.h"
#include <string.h>

namespace
{
    std::vector<uint8_t> get_bytes(const void*    in_data_ptr,
                                   const uint32_t& in_n_bytes)
    {
        const auto* byte_ptr = static_cast<const uint8_t*>(in_data_ptr);

        return std::vector<uint8_t>(byte_ptr,
                                    byte_ptr + in_n_bytes);
    }

    /* The camera is 4 from the origin of the world. */
    void set_camera(Tests::D3D8Scene* inout_scene_ptr)
    {
        Interception::Hooks::D3D8::D3DMATRIX matrix = {};

        matrix.m[0][0] = 1.0f;
        matrix.m[1][1] = 1.0f;
        matrix.m[2][2] = 1.0f;
        matrix.m[3][2] = 4.0f;
        matrix.m[3][3] = 1.0f;

        inout_scene_ptr->get_builder().set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                                                     matrix);
    }
}

TEST(D3D8FrontendGeometry, TheVerticesOfADrawAreWhatWasWrittenToItsBuffers)
{
    constexpr uint32_t STRIDE = 36;

    Tests::D3D8Scene scene;
    float            vertex_array[4 * STRIDE / sizeof(float)] = {};
    const uint16_t   index_array[6]                           = {0, 1, 2, 2, 1, 3};

    /* The first three floats of each vertex are its position: all of them 6 beyond the origin of the world. */
    for (uint32_t n_vertex = 0;
                  n_vertex < 4;
                ++n_vertex)
    {
        vertex_array[n_vertex * STRIDE / sizeof(float) + 2] = 6.0f;
    }

    scene.get_builder().lock_vertex_buffer  (scene.get_vertex_buffer(),
                                             0,
                                             sizeof(vertex_array) );
    scene.get_builder().unlock_vertex_buffer(scene.get_vertex_buffer(),
                                             get_bytes(vertex_array,
                                                       sizeof(vertex_array) ));
    scene.get_builder().lock_index_buffer   (scene.get_index_buffer(),
                                             0,
                                             sizeof(index_array) );
    scene.get_builder().unlock_index_buffer (scene.get_index_buffer(),
                                             get_bytes(index_array,
                                                       sizeof(index_array) ));
    set_camera(&scene);
    scene.bind_back_buffer();
    scene.draw(1);
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);

    /* Beyond the origin, and the origin is 4 from the camera. */
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                10.0f,
                1.0f);
    EXPECT_NEAR(model.depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                4.0f,
                0.5f);
}

TEST(D3D8FrontendGeometry, TheIndicesPickTheVertexThatWasWrittenAndTheRestOfTheBufferIsZeros)
{
    constexpr uint32_t STRIDE = 36;

    Tests::D3D8Scene scene;
    float            vertex_array[STRIDE / sizeof(float)] = {};
    const uint16_t   index_array[6]                       = {0, 0, 0, 0, 0, 0};

    vertex_array[2] = 20.0f;

    /* Only the first vertex is written, and every index is 0. */
    scene.get_builder().lock_vertex_buffer  (scene.get_vertex_buffer(),
                                             0,
                                             sizeof(vertex_array) );
    scene.get_builder().unlock_vertex_buffer(scene.get_vertex_buffer(),
                                             get_bytes(vertex_array,
                                                       sizeof(vertex_array) ));
    scene.get_builder().lock_index_buffer   (scene.get_index_buffer(),
                                             0,
                                             sizeof(index_array) );
    scene.get_builder().unlock_index_buffer (scene.get_index_buffer(),
                                             get_bytes(index_array,
                                                       sizeof(index_array) ));
    set_camera(&scene);
    scene.bind_back_buffer();
    scene.draw(1);
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                24.0f,
                2.4f);
}

TEST(D3D8FrontendGeometry, WithoutWhatWasWrittenToTheBuffersADrawHasTheirZeros)
{
    Tests::D3D8Scene scene;

    set_camera(&scene);
    scene.bind_back_buffer();
    scene.draw(1);
    scene.present();

    const auto& model = scene.get_harness().get_model();

    /* The vertices are all at the origin of the world, which is 4 from the camera. */
    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                4.0f,
                0.5f);
}

TEST(D3D8FrontendGeometry, PreTransformedVerticesOfABufferAreSampledByTheReciprocalOfTheirDepthAndNotMovedByAMatrix)
{
    constexpr uint32_t FVF_XYZRHW = 0x004;
    constexpr uint32_t STRIDE     = 16;

    Tests::D3D8Scene scene;

    /* x and y in pixels, the depth of the buffer, and the reciprocal of the depth: 1, so the depth is 1. */
    const float vertex_array[12] = {10.0f, 10.0f, 0.5f, 1.0f,
                                    20.0f, 10.0f, 0.5f, 1.0f,
                                    10.0f, 20.0f, 0.5f, 1.0f};

    scene.get_builder().set_vertex_shader   (FVF_XYZRHW);
    scene.get_builder().set_stream_source   (0,
                                             scene.get_vertex_buffer(),
                                             STRIDE);
    scene.get_builder().lock_vertex_buffer  (scene.get_vertex_buffer(),
                                             0,
                                             sizeof(vertex_array) );
    scene.get_builder().unlock_vertex_buffer(scene.get_vertex_buffer(),
                                             get_bytes(vertex_array,
                                                       sizeof(vertex_array) ));
    set_camera(&scene);
    scene.bind_back_buffer();
    scene.get_builder().draw_primitive(Interception::Hooks::D3D8::D3DPT_TRIANGLELIST,
                                       0,
                                       1);
    scene.feed();
    scene.present();

    const auto& model = scene.get_harness().get_model();

    /* The camera is 4 away, which has nothing to do with it. */
    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                1.0f,
                0.2f);
}

TEST(D3D8FrontendGeometry, TheVerticesOfADrawFromMemoryAreThoseThatCameWithTheCall)
{
    constexpr uint32_t FVF_XYZ = 0x002;
    constexpr uint32_t STRIDE  = 12;

    Tests::D3D8Scene scene;

    /* Three vertices 6 beyond the origin of the world. */
    const float vertex_array[9] = {0.0f, 0.0f, 6.0f,
                                   1.0f, 0.0f, 6.0f,
                                   0.0f, 1.0f, 6.0f};

    scene.get_builder().set_vertex_shader(FVF_XYZ);
    set_camera(&scene);
    scene.bind_back_buffer();
    scene.get_builder().draw_primitive_up(Interception::Hooks::D3D8::D3DPT_TRIANGLELIST,
                                          1,
                                          STRIDE,
                                          get_bytes(vertex_array,
                                                    sizeof(vertex_array) ));
    scene.feed();
    scene.present();

    const auto& model = scene.get_harness().get_model();

    /* The camera is 4 from the origin: the vertices are 10 from it. */
    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                10.0f,
                1.0f);
}

TEST(D3D8FrontendGeometry, PreTransformedVerticesOfADrawFromMemoryAreSampledByTheReciprocalOfTheirDepth)
{
    constexpr uint32_t FVF_XYZRHW = 0x004;
    constexpr uint32_t STRIDE     = 16;

    Tests::D3D8Scene scene;

    const float vertex_array[12] = {10.0f, 10.0f, 0.5f, 1.0f,
                                    20.0f, 10.0f, 0.5f, 1.0f,
                                    10.0f, 20.0f, 0.5f, 1.0f};

    scene.get_builder().set_vertex_shader(FVF_XYZRHW);
    set_camera(&scene);
    scene.bind_back_buffer();
    scene.get_builder().draw_primitive_up(Interception::Hooks::D3D8::D3DPT_TRIANGLELIST,
                                          1,
                                          STRIDE,
                                          get_bytes(vertex_array,
                                                    sizeof(vertex_array) ));
    scene.feed();
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                1.0f,
                0.2f);
}

TEST(D3D8FrontendGeometry, TheIndicesOfAnIndexedDrawFromMemoryPickTheVerticesThatAreSampled)
{
    constexpr uint32_t FORMAT_INDEX16 = 101;
    constexpr uint32_t FVF_XYZRHW     = 0x004;
    constexpr uint32_t STRIDE         = 16;

    Tests::D3D8Scene scene;

    /* The first vertex is far (the reciprocal of 1000 is small), and the indices do not use it. */
    const float vertex_array[16] = {10.0f, 10.0f, 0.5f, 0.001f,
                                    10.0f, 10.0f, 0.5f, 1.0f,
                                    20.0f, 10.0f, 0.5f, 1.0f,
                                    10.0f, 20.0f, 0.5f, 1.0f};
    const uint16_t index_array[3] = {1, 2, 3};

    scene.get_builder().set_vertex_shader(FVF_XYZRHW);
    set_camera(&scene);
    scene.bind_back_buffer();
    scene.get_builder().draw_indexed_primitive_up(Interception::Hooks::D3D8::D3DPT_TRIANGLELIST,
                                                  0,
                                                  4,
                                                  1,
                                                  get_bytes(index_array,
                                                            sizeof(index_array) ),
                                                  FORMAT_INDEX16,
                                                  get_bytes(vertex_array,
                                                            sizeof(vertex_array) ),
                                                  STRIDE);
    scene.feed();
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                1.0f,
                0.2f);
}

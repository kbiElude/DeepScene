/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the parallax of vertices that are already in the space of the picture: how far a vertex is moved for each eye, and that a draw of such
 * vertices is the only call that is changed.
 */
#include <gtest/gtest.h>
#include "d3d7_draw_rewriter.h"
#include <map>
#include <math.h>
#include <string.h>

namespace
{
    using namespace Stereo::Replay::EngineDLL;

    constexpr uint32_t FVF_PRETRANSFORMED = 0x1c4; /* XYZRHW, a diffuse and a specular color, one set of texture coordinates: 32 bytes */
    constexpr uint32_t FVF_TRANSFORMED_BY_MATRIX = 0x142; /* XYZ, a diffuse color, one set of texture coordinates */
    constexpr size_t   VERTEX_N_BYTES     = 32;

    /* Chunks which are kept in a map. */
    class MapChunkSource : public IChunkSource
    {
    public:
        const Capture::APIDumpLoader::DataChunk* get_data_chunk_ptr(const uint32_t& in_data_chunk_id) const override
        {
            const auto chunk_iterator = chunk_map.find(in_data_chunk_id);

            return (chunk_iterator != chunk_map.end() ) ? &chunk_iterator->second
                                                        : nullptr;
        }

        uint32_t get_pointer_n_bytes() const override
        {
            return 4;
        }

        std::map<uint32_t, Capture::APIDumpLoader::DataChunk> chunk_map;
    };

    Stereo::D3D7::ScreenParallax::Parameters make_parameters(const Analysis::RecipeLoader::Eye& in_eye)
    {
        Stereo::D3D7::ScreenParallax::Parameters result;

        result.convergence_distance  = 5.0f;
        result.eye                   = in_eye;
        result.eye_separation        = 2.0f;
        result.field_of_view_degrees = 90.0f;
        result.picture_width         = 640.0f;

        return result;
    }

    /* Vertices: x, y, z, the reciprocal of the depth, a diffuse color, a specular color, and one pair of texture coordinates. */
    Capture::APIDumpLoader::DataChunk make_vertices(const std::vector<float>& in_x_vec,
                                                    const std::vector<float>& in_reciprocal_vec)
    {
        Capture::APIDumpLoader::DataChunk result(in_x_vec.size() * VERTEX_N_BYTES);

        for (size_t n_vertex = 0;
                    n_vertex < in_x_vec.size();
                  ++n_vertex)
        {
            float vertex_f32vec8[8] = {in_x_vec.at(n_vertex),
                                       100.0f + static_cast<float>(n_vertex),
                                       0.5f,
                                       in_reciprocal_vec.at(n_vertex),
                                       0.0f,
                                       0.0f,
                                       0.25f,
                                       0.75f};

            ::memcpy(result.data() + n_vertex * VERTEX_N_BYTES,
                     vertex_f32vec8,
                     sizeof(vertex_f32vec8) );
        }

        return result;
    }

    float get_x(const Capture::APIDumpLoader::DataChunk& in_vertices,
                const size_t&                            in_n_vertex)
    {
        float result = 0.0f;

        ::memcpy(&result,
                 in_vertices.data() + in_n_vertex * VERTEX_N_BYTES,
                 sizeof(result) );

        return result;
    }

    /* DrawPrimitive(device, type, format, vertices, n vertices, flags) and the vertices as a data chunk. */
    Capture::APIDumpLoader::DumpedAPICall make_draw_primitive(const uint32_t& in_fvf,
                                                              const uint32_t& in_n_vertices,
                                                              const uint32_t& in_chunk_id)
    {
        Capture::APIDumpLoader::DumpedAPICall result;

        result.func = APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE;

        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u64(0x1000) );
        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(5) );
        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(in_fvf) );
        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u64(0x2000) );
        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(in_n_vertices) );
        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(0) );
        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_data_chunk_id(in_chunk_id) );

        return result;
    }
}

TEST(D3D7ScreenParallax, AVertexAtTheConvergenceDistanceIsNotMovedAndThoseNearerAndFurtherAreMovedTheOtherWayForEachEye)
{
    const auto left  = make_parameters(Analysis::RecipeLoader::Eye::LEFT);
    const auto right = make_parameters(Analysis::RecipeLoader::Eye::RIGHT);

    const float convergence_reciprocal = 1.0f / 5.0f;

    EXPECT_FLOAT_EQ(Stereo::D3D7::ScreenParallax::get_shift(left,
                                                  convergence_reciprocal),
                    0.0f);
    EXPECT_FLOAT_EQ(Stereo::D3D7::ScreenParallax::get_shift(right,
                                                  convergence_reciprocal),
                    0.0f);

    /* Nearer: the left eye sees it further to the right (it is moved to the left), the right eye further to the left. */
    const float near_reciprocal = 1.0f;

    EXPECT_GT(Stereo::D3D7::ScreenParallax::get_shift(left,
                                            near_reciprocal),
              0.0f);
    EXPECT_FLOAT_EQ(Stereo::D3D7::ScreenParallax::get_shift(right,
                                                  near_reciprocal),
                    -Stereo::D3D7::ScreenParallax::get_shift(left,
                                                   near_reciprocal) );

    /* Further away: the other way. */
    const float far_reciprocal = 0.01f;

    EXPECT_LT(Stereo::D3D7::ScreenParallax::get_shift(left,
                                            far_reciprocal),
              0.0f);
    EXPECT_GT(Stereo::D3D7::ScreenParallax::get_shift(right,
                                            far_reciprocal),
              0.0f);
}

TEST(D3D7ScreenParallax, TheShiftIsThatOfACameraMovedHalfTheSeparationToTheSide)
{
    const auto  left            = make_parameters(Analysis::RecipeLoader::Eye::LEFT);
    const float reciprocal      = 0.5f; /* a depth of 2 */
    const float focal_length    = 320.0f / tanf(45.0f * 3.14159265358979f / 180.0f); /* half the width, over the tangent of half the angle of view */
    const float expected_shift  = focal_length * (2.0f / 2.0f) * (reciprocal - 0.2f);

    EXPECT_NEAR(Stereo::D3D7::ScreenParallax::get_shift(left,
                                              reciprocal),
                expected_shift,
                0.001f);

    /* Twice the separation, twice the shift; a narrower angle of view, a larger one. */
    auto twice_apart = left;

    twice_apart.eye_separation = 4.0f;

    EXPECT_NEAR(Stereo::D3D7::ScreenParallax::get_shift(twice_apart,
                                              reciprocal),
                2.0f * expected_shift,
                0.001f);

    auto narrow = left;

    narrow.field_of_view_degrees = 45.0f;

    EXPECT_GT(Stereo::D3D7::ScreenParallax::get_shift(narrow,
                                            reciprocal),
              expected_shift);
}

TEST(D3D7ScreenParallax, WhatIsBehindTheCameraOrNotANumberIsPutWhereItCannotGoWrong)
{
    const auto left = make_parameters(Analysis::RecipeLoader::Eye::LEFT);

    /* No depth, or a negative one, is the same as the furthest there is. */
    EXPECT_FLOAT_EQ(Stereo::D3D7::ScreenParallax::get_shift(left,
                                                  -7.0f),
                    Stereo::D3D7::ScreenParallax::get_shift(left,
                                                  0.0f) );
    EXPECT_FLOAT_EQ(Stereo::D3D7::ScreenParallax::get_shift(left,
                                                  nanf("") ),
                    Stereo::D3D7::ScreenParallax::get_shift(left,
                                                  0.0f) );

    /* And what is too near is where the nearest allowed is: a tenth of the convergence distance. */
    EXPECT_FLOAT_EQ(Stereo::D3D7::ScreenParallax::get_shift(left,
                                                  1.0e9f),
                    Stereo::D3D7::ScreenParallax::get_shift(left,
                                                  2.0f) ); /* the reciprocal of a depth of 0.5 */
    EXPECT_TRUE(isfinite(Stereo::D3D7::ScreenParallax::get_shift(left,
                                                       INFINITY) ) );
}

TEST(D3D7ScreenParallax, OnlyTheXOfEachVertexIsMoved)
{
    const auto vertices_before = make_vertices({10.0f, 200.0f, 300.0f},
                                               {1.0f, 0.2f, 0.01f});
    auto       vertices        = vertices_before;

    Stereo::D3D7::ScreenParallax::shift_vertices(make_parameters(Analysis::RecipeLoader::Eye::LEFT),
                                       VERTEX_N_BYTES,
                                       3,
                                       vertices.data() );

    EXPECT_GT(get_x(vertices,
                    0),
              10.0f);
    EXPECT_FLOAT_EQ(get_x(vertices,
                          1),
                    200.0f); /* at the convergence distance */
    EXPECT_LT(get_x(vertices,
                    2),
              300.0f);

    /* Everything else of every vertex is as it was. */
    for (size_t n_vertex = 0;
                n_vertex < 3;
              ++n_vertex)
    {
        EXPECT_EQ(::memcmp(vertices.data()        + n_vertex * VERTEX_N_BYTES + sizeof(float),
                           vertices_before.data() + n_vertex * VERTEX_N_BYTES + sizeof(float),
                           VERTEX_N_BYTES - sizeof(float) ),
                  0) << "vertex " << n_vertex;
    }
}

TEST(D3D7DrawRewriter, ADrawOfPreTransformedVerticesIsCopiedWithTheVerticesMovedAndTheOriginalLeftAlone)
{
    MapChunkSource                                 source;
    OverlayChunkSource                             overlay(source);
    Capture::APIDumpLoader::DumpedAPICall          rewritten;
    const auto                                     original_vertices = make_vertices({10.0f, 20.0f},
                                                                                     {1.0f, 0.2f});

    source.chunk_map[7] = original_vertices;

    const auto call = make_draw_primitive(FVF_PRETRANSFORMED,
                                          2,
                                          7);

    ASSERT_TRUE(D3D7DrawRewriter::rewrite(call,
                                            source,
                                            make_parameters(Analysis::RecipeLoader::Eye::LEFT),
                                           &overlay,
                                           &rewritten) );

    /* The call is the same but for where the vertices are. */
    ASSERT_EQ(rewritten.arg_in_vec.size(),
              call.arg_in_vec.size() );
    EXPECT_EQ(rewritten.func,
              call.func);
    EXPECT_NE(rewritten.arg_in_vec.back().get_data_chunk_id(),
              7u);
    EXPECT_EQ(rewritten.arg_in_vec.at(2).get_u32(),
              FVF_PRETRANSFORMED);

    const auto* moved_ptr = overlay.get_data_chunk_ptr(rewritten.arg_in_vec.back().get_data_chunk_id() );

    ASSERT_NE(moved_ptr,
              nullptr);
    EXPECT_GT(get_x(*moved_ptr,
                    0),
              10.0f);
    EXPECT_FLOAT_EQ(get_x(*moved_ptr,
                          1),
                    20.0f);

    /* What the workload holds is not touched: the other eye is made from it as well. */
    EXPECT_FLOAT_EQ(get_x(*source.get_data_chunk_ptr(7),
                          0),
                    10.0f);

    /* The right eye is the other way. */
    Capture::APIDumpLoader::DumpedAPICall right_rewritten;
    OverlayChunkSource                    right_overlay(source);

    ASSERT_TRUE(D3D7DrawRewriter::rewrite(call,
                                            source,
                                            make_parameters(Analysis::RecipeLoader::Eye::RIGHT),
                                           &right_overlay,
                                           &right_rewritten) );
    EXPECT_LT(get_x(*right_overlay.get_data_chunk_ptr(right_rewritten.arg_in_vec.back().get_data_chunk_id() ),
                    0),
              10.0f);
}

TEST(D3D7DrawRewriter, ADrawPushedAsAWholeHasEveryVertexMovedByTheSameShiftWhateverItsDepth)
{
    MapChunkSource                        source;
    OverlayChunkSource                    overlay(source);
    Capture::APIDumpLoader::DumpedAPICall rewritten;

    source.chunk_map[7] = make_vertices({10.0f, 20.0f},
                                        {1.0f,  0.2f});

    const auto call = make_draw_primitive(FVF_PRETRANSFORMED,
                                          2,
                                          7);

    ASSERT_TRUE(D3D7DrawRewriter::rewrite_by(call,
                                               source,
                                               -4.0f,
                                              &overlay,
                                              &rewritten) );

    const auto* moved_ptr = overlay.get_data_chunk_ptr(rewritten.arg_in_vec.back().get_data_chunk_id() );

    ASSERT_NE(moved_ptr,
              nullptr);
    EXPECT_FLOAT_EQ(get_x(*moved_ptr,
                          0),
                    6.0f);
    EXPECT_FLOAT_EQ(get_x(*moved_ptr,
                          1),
                    16.0f);
    EXPECT_FLOAT_EQ(get_x(*source.get_data_chunk_ptr(7),
                          0),
                    10.0f);
}

TEST(D3D7DrawRewriter, ADrawPushedByNothingOrOfVerticesAMatrixMovesIsLeftAlone)
{
    MapChunkSource                        source;
    OverlayChunkSource                    overlay(source);
    Capture::APIDumpLoader::DumpedAPICall rewritten;

    source.chunk_map[7] = make_vertices({10.0f},
                                        {1.0f});

    EXPECT_FALSE(D3D7DrawRewriter::rewrite_by(make_draw_primitive(FVF_PRETRANSFORMED,
                                                                    1,
                                                                    7),
                                                source,
                                                0.0f,
                                               &overlay,
                                               &rewritten) );
    EXPECT_FALSE(D3D7DrawRewriter::rewrite_by(make_draw_primitive(FVF_TRANSFORMED_BY_MATRIX,
                                                                    1,
                                                                    7),
                                                source,
                                                3.0f,
                                               &overlay,
                                               &rewritten) );
}

TEST(D3D7DrawRewriter, ADrawWhichTheIndicesComeBeforeTheVerticesOfHasTheVerticesMovedAndTheIndicesAsTheyAre)
{
    MapChunkSource                        source;
    OverlayChunkSource                    overlay(source);
    Capture::APIDumpLoader::DumpedAPICall rewritten;

    source.chunk_map[3] = Capture::APIDumpLoader::DataChunk{0, 0, 1, 0, 2, 0};
    source.chunk_map[4] = make_vertices({10.0f, 20.0f, 30.0f},
                                        {1.0f, 1.0f, 1.0f});

    /* DrawIndexedPrimitive(device, type, format, vertices, n vertices, indices, n indices, flags), the indices, and the vertices. */
    Capture::APIDumpLoader::DumpedAPICall call;

    call.func = APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE;

    call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u64(0x1000) );
    call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(4) );
    call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(FVF_PRETRANSFORMED) );
    call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u64(0x2000) );
    call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(3) );
    call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u64(0x3000) );
    call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(3) );
    call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(0) );
    call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_data_chunk_id(3) );
    call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_data_chunk_id(4) );

    ASSERT_TRUE(D3D7DrawRewriter::rewrite(call,
                                            source,
                                            make_parameters(Analysis::RecipeLoader::Eye::LEFT),
                                           &overlay,
                                           &rewritten) );

    EXPECT_EQ(rewritten.arg_in_vec.at(8).get_data_chunk_id(),
              3u);
    EXPECT_NE(rewritten.arg_in_vec.at(9).get_data_chunk_id(),
              4u);
    EXPECT_GT(get_x(*overlay.get_data_chunk_ptr(rewritten.arg_in_vec.at(9).get_data_chunk_id() ),
                    2),
              30.0f);
}

TEST(D3D7DrawRewriter, WhatIsNotADrawOfPreTransformedVerticesWhichThereIsRoomForIsLeftAlone)
{
    MapChunkSource                        source;
    OverlayChunkSource                    overlay(source);
    Capture::APIDumpLoader::DumpedAPICall rewritten;

    source.chunk_map[7] = make_vertices({10.0f, 20.0f},
                                        {1.0f, 0.2f});

    /* Vertices which a matrix moves. */
    EXPECT_FALSE(D3D7DrawRewriter::rewrite(make_draw_primitive(FVF_TRANSFORMED_BY_MATRIX,
                                                                 2,
                                                                 7),
                                             source,
                                             make_parameters(Analysis::RecipeLoader::Eye::LEFT),
                                            &overlay,
                                            &rewritten) );

    /* Another call. */
    auto clear_call = make_draw_primitive(FVF_PRETRANSFORMED,
                                          2,
                                          7);

    clear_call.func = APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR;

    EXPECT_FALSE(D3D7DrawRewriter::rewrite(clear_call,
                                             source,
                                             make_parameters(Analysis::RecipeLoader::Eye::LEFT),
                                            &overlay,
                                            &rewritten) );

    /* More vertices than the chunk holds, and a chunk the workload does not have. */
    EXPECT_FALSE(D3D7DrawRewriter::rewrite(make_draw_primitive(FVF_PRETRANSFORMED,
                                                                 3,
                                                                 7),
                                             source,
                                             make_parameters(Analysis::RecipeLoader::Eye::LEFT),
                                            &overlay,
                                            &rewritten) );
    EXPECT_FALSE(D3D7DrawRewriter::rewrite(make_draw_primitive(FVF_PRETRANSFORMED,
                                                                 2,
                                                                 99),
                                             source,
                                             make_parameters(Analysis::RecipeLoader::Eye::LEFT),
                                            &overlay,
                                            &rewritten) );

    /* Eyes which are not apart have nothing to be moved by. */
    auto together = make_parameters(Analysis::RecipeLoader::Eye::LEFT);

    together.eye_separation = 0.0f;

    EXPECT_FALSE(D3D7DrawRewriter::rewrite(make_draw_primitive(FVF_PRETRANSFORMED,
                                                                 2,
                                                                 7),
                                             source,
                                             together,
                                            &overlay,
                                            &rewritten) );
}

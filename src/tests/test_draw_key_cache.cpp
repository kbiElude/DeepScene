/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The cache of the keys of a draw: what decides a hit, and that a model builder with the cache works out the same keys as one without it.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/draw_key_cache.h"
#include "model_test_helper.h"
#include <array>
#include <utility>
#include <vector>

namespace
{
    typedef std::pair<uint64_t, uint64_t> KeyPair;

    Analysis::APIDumpAnalysis::DrawDescription make_draw()
    {
        Analysis::APIDumpAnalysis::DrawDescription result;
        Analysis::APIDumpAnalysis::TextureBinding  binding;

        binding.view.resource_id = Analysis::APIDumpAnalysis::make_resource_id(7);

        result.index_buffer_id      = Analysis::APIDumpAnalysis::make_resource_id(3);
        result.is_indexed           = true;
        result.primitive_topology   = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST;
        result.state_key            = 11;
        result.vertex_layout_key    = 12;
        result.vertex_count         = 30;

        result.shader_id_vec.push_back       (Analysis::APIDumpAnalysis::make_resource_id(5) );
        result.texture_binding_vec.push_back (binding);
        result.vertex_buffer_id_vec.push_back(Analysis::APIDumpAnalysis::make_resource_id(4) );

        return result;
    }

    Analysis::APIDumpAnalysis::AttachmentSet make_attachments()
    {
        Analysis::APIDumpAnalysis::AttachmentSet result;
        Analysis::APIDumpAnalysis::ResourceView  color;

        color.resource_id = Analysis::APIDumpAnalysis::make_resource_id(1);

        result.color_view_vec.push_back(color);

        result.depth_stencil_view.resource_id = Analysis::APIDumpAnalysis::make_resource_id(2);

        return result;
    }

    Analysis::APIDumpAnalysis::DrawKeyResult make_result()
    {
        Analysis::APIDumpAnalysis::DrawKeyResult result;

        result.is_collision  = true;
        result.signature_key = 101;
        result.structure_key = 102;

        return result;
    }

    /* Whether a cache that remembers a draw knows the draw changed by the function. */
    template<typename Func>
    bool is_change_found(const Func& in_change_func)
    {
        Analysis::APIDumpAnalysis::DrawKeyCache  cache;
        Analysis::APIDumpAnalysis::DrawKeyResult found;
        auto                                     draw        = make_draw();
        auto                                     attachments = make_attachments();
        uint64_t                                 epoch       = 5;
        bool                                     is_ortho    = false;

        cache.store(draw,
                    attachments,
                    epoch,
                    is_ortho,
                    make_result() );

        in_change_func(&draw,
                       &attachments,
                       &epoch,
                       &is_ortho);

        return cache.find(draw,
                          attachments,
                          epoch,
                          is_ortho,
                         &found);
    }
}

TEST(DrawKeyCache, ADrawThatWasNeverStoredIsNotFound)
{
    Analysis::APIDumpAnalysis::DrawKeyCache  cache;
    Analysis::APIDumpAnalysis::DrawKeyResult found;

    EXPECT_FALSE(cache.find(make_draw(),
                            make_attachments(),
                            0,
                            false,
                           &found) );
}

TEST(DrawKeyCache, ARepeatedDrawIsFoundWithItsKeys)
{
    Analysis::APIDumpAnalysis::DrawKeyCache  cache;
    Analysis::APIDumpAnalysis::DrawKeyResult found;

    cache.store(make_draw(),
                make_attachments(),
                5,
                false,
                make_result() );

    ASSERT_TRUE(cache.find(make_draw(),
                           make_attachments(),
                           5,
                           false,
                          &found) );
    EXPECT_TRUE(found.is_collision);
    EXPECT_EQ  (found.signature_key,
                101u);
    EXPECT_EQ  (found.structure_key,
                102u);
}

TEST(DrawKeyCache, WhatIsNotPartOfTheKeysDoesNotMakeADifferentDraw)
{
    EXPECT_TRUE(is_change_found([](Analysis::APIDumpAnalysis::DrawDescription* draw_ptr, Analysis::APIDumpAnalysis::AttachmentSet*, uint64_t*, bool*)
                                {
                                    draw_ptr->vertex_count         = 3000;
                                    draw_ptr->first_call_index     = 17;
                                    draw_ptr->view_anchor_call_index = 18;
                                    draw_ptr->reads_depth_stencil  = true;
                                }) );
    EXPECT_TRUE(is_change_found([](Analysis::APIDumpAnalysis::DrawDescription* draw_ptr, Analysis::APIDumpAnalysis::AttachmentSet*, uint64_t*, bool*)
                                {
                                    draw_ptr->instance_count = 1;
                                }) );
}

TEST(DrawKeyCache, EverythingThatIsPartOfTheKeysMakesADifferentDraw)
{
    typedef Analysis::APIDumpAnalysis::DrawDescription Draw;
    typedef Analysis::APIDumpAnalysis::AttachmentSet   Attachments;

    EXPECT_FALSE(is_change_found([](Draw* draw_ptr, Attachments*, uint64_t*, bool*) { draw_ptr->state_key += 1; }) );
    EXPECT_FALSE(is_change_found([](Draw* draw_ptr, Attachments*, uint64_t*, bool*) { draw_ptr->vertex_layout_key += 1; }) );
    EXPECT_FALSE(is_change_found([](Draw* draw_ptr, Attachments*, uint64_t*, bool*) { draw_ptr->is_indexed = false; }) );
    EXPECT_FALSE(is_change_found([](Draw* draw_ptr, Attachments*, uint64_t*, bool*) { draw_ptr->instance_count = 2; }) );
    EXPECT_FALSE(is_change_found([](Draw* draw_ptr, Attachments*, uint64_t*, bool*) { draw_ptr->primitive_topology = Analysis::APIDumpAnalysis::PrimitiveTopology::LINE_LIST; }) );
    EXPECT_FALSE(is_change_found([](Draw* draw_ptr, Attachments*, uint64_t*, bool*) { draw_ptr->index_buffer_id = Analysis::APIDumpAnalysis::make_resource_id(9); }) );
    EXPECT_FALSE(is_change_found([](Draw* draw_ptr, Attachments*, uint64_t*, bool*) { draw_ptr->shader_id_vec.push_back(Analysis::APIDumpAnalysis::make_resource_id(9) ); }) );
    EXPECT_FALSE(is_change_found([](Draw* draw_ptr, Attachments*, uint64_t*, bool*) { draw_ptr->vertex_buffer_id_vec.clear(); }) );
    EXPECT_FALSE(is_change_found([](Draw* draw_ptr, Attachments*, uint64_t*, bool*) { draw_ptr->texture_binding_vec.at(0).view.mip_level = 1; }) );
    EXPECT_FALSE(is_change_found([](Draw* draw_ptr, Attachments*, uint64_t*, bool*) { draw_ptr->texture_binding_vec.at(0).binding_point.index = 1; }) );
    EXPECT_FALSE(is_change_found([](Draw*, Attachments* attachments_ptr, uint64_t*, bool*) { attachments_ptr->color_view_vec.at(0).layer = 1; }) );
    EXPECT_FALSE(is_change_found([](Draw*, Attachments* attachments_ptr, uint64_t*, bool*) { attachments_ptr->color_view_vec.clear(); }) );
    EXPECT_FALSE(is_change_found([](Draw*, Attachments* attachments_ptr, uint64_t*, bool*) { attachments_ptr->depth_stencil_view.resource_id = Analysis::APIDumpAnalysis::ResourceId::NONE; }) );
    EXPECT_FALSE(is_change_found([](Draw*, Attachments*, uint64_t* epoch_ptr, bool*) { *epoch_ptr += 1; }) );
    EXPECT_FALSE(is_change_found([](Draw*, Attachments*, uint64_t*, bool* is_orthographic_ptr) { *is_orthographic_ptr = true; }) );
}

TEST(DrawKeyCache, TheCacheKeepsTheLastFewDrawsAndForgetsTheOldestFirst)
{
    constexpr size_t N_ENTRIES = 4;

    Analysis::APIDumpAnalysis::DrawKeyCache  cache;
    Analysis::APIDumpAnalysis::DrawKeyResult found;
    auto                                     attachments = make_attachments();

    for (size_t n_draw = 0;
                n_draw < N_ENTRIES + 1;
              ++n_draw)
    {
        auto draw = make_draw();

        draw.state_key = 1000 + n_draw;

        cache.store(draw,
                    attachments,
                    5,
                    false,
                    make_result() );
    }

    for (size_t n_draw = 0;
                n_draw < N_ENTRIES + 1;
              ++n_draw)
    {
        auto draw = make_draw();

        draw.state_key = 1000 + n_draw;

        /* The first draw is the one that the fifth took the place of. */
        EXPECT_EQ(cache.find(draw,
                             attachments,
                             5,
                             false,
                            &found),
                  n_draw != 0);
    }
}

TEST(DrawKeyCache, ABuilderWithTheCacheWorksOutTheKeysOfABuilderWithoutIt)
{
    constexpr uint32_t N_STEPS = 6000;

    const std::array<float, 16> orthographic_f32mat = {1.0f, 0.0f, 0.0f, 0.0f,
                                                       0.0f, 1.0f, 0.0f, 0.0f,
                                                       0.0f, 0.0f, 1.0f, 0.0f,
                                                       0.0f, 0.0f, 0.0f, 1.0f};
    const std::array<float, 16> perspective_f32mat  = {1.0f, 0.0f, 0.0f,  0.0f,
                                                       0.0f, 1.0f, 0.0f,  0.0f,
                                                       0.0f, 0.0f, 1.0f, -1.0f,
                                                       0.0f, 0.0f, 0.0f,  0.0f};

    std::vector<KeyPair>  key_vec  [2];
    uint64_t              n_collisions_array[2] = {};
    size_t                n_structures_array [2] = {};

    for (uint32_t n_run = 0;
                  n_run < 2;
                ++n_run)
    {
        Tests::ModelTestHelper helper;

        helper.get_builder().set_caches_draw_keys(n_run == 0);

        const auto color_id_array = std::array<Analysis::APIDumpAnalysis::ResourceId, 3>{helper.create_swapchain_image(),
                                                                                        helper.create_color_texture(),
                                                                                        helper.create_color_texture()};
        const auto depth_id       = helper.create_depth_buffer();
        const auto static_id      = helper.create_static_texture();
        uint64_t   random_state   = 12345;

        const auto next_random_func = [&random_state]()
        {
            random_state = random_state * 6364136223846793005ull + 1442695040888963407ull;

            return static_cast<uint32_t>(random_state >> 33);
        };

        helper.bind(color_id_array.at(0),
                    depth_id);

        for (uint32_t n_step = 0;
                      n_step < N_STEPS;
                    ++n_step)
        {
            const uint32_t action = next_random_func() % 24;

            switch (action)
            {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                {
                    helper.draw(next_random_func() % 3,
                                3 + next_random_func() % 7);

                    key_vec[n_run].push_back({helper.get_builder().get_last_draw_signature_key(),
                                              helper.get_builder().get_last_draw_cover_key() });

                    break;
                }

                case 8:
                case 9:
                case 10:
                case 11:
                {
                    helper.draw_textured(color_id_array.at(next_random_func() % 3),
                                         next_random_func() % 2);

                    key_vec[n_run].push_back({helper.get_builder().get_last_draw_signature_key(),
                                              helper.get_builder().get_last_draw_cover_key() });

                    break;
                }

                case 12:
                {
                    helper.draw_textured(static_id,
                                         0);

                    key_vec[n_run].push_back({helper.get_builder().get_last_draw_signature_key(),
                                              helper.get_builder().get_last_draw_cover_key() });

                    break;
                }

                case 13:
                {
                    helper.bind(color_id_array.at(next_random_func() % 3),
                                ((next_random_func() % 2) == 0) ? depth_id
                                                                : Analysis::APIDumpAnalysis::ResourceId::NONE);

                    break;
                }

                case 14:
                {
                    helper.clear(next_random_func() % 2 == 0,
                                 next_random_func() % 2 == 0);

                    break;
                }

                case 15:
                {
                    helper.upload(static_id);

                    break;
                }

                case 16:
                {
                    helper.copy(color_id_array.at(1),
                                color_id_array.at(2) );

                    break;
                }

                case 17:
                case 18:
                {
                    helper.set_matrix(Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX,
                                      192,
                                      ((next_random_func() % 2) == 0) ? orthographic_f32mat
                                                                      : perspective_f32mat);

                    break;
                }

                case 19:
                {
                    helper.set_view_matrix(static_cast<uint8_t>(next_random_func() % 4) );

                    break;
                }

                case 20:
                {
                    helper.end_frame();

                    break;
                }

                default:
                {
                    /* Draws that repeat the last one, as a run of objects with the same material does. */
                    helper.draw(1,
                                6);

                    key_vec[n_run].push_back({helper.get_builder().get_last_draw_signature_key(),
                                              helper.get_builder().get_last_draw_cover_key() });

                    break;
                }
            }
        }

        helper.end_frame();

        n_collisions_array[n_run] = helper.get_model().n_key_collisions;
        n_structures_array [n_run] = helper.get_model().draw_structure_map.size();
    }

    ASSERT_GT(key_vec[0].size(),
              1000u);
    EXPECT_EQ(key_vec[0],
              key_vec[1]);
    EXPECT_EQ(n_collisions_array[0],
              n_collisions_array[1]);
    EXPECT_EQ(n_structures_array[0],
              n_structures_array[1]);
    EXPECT_GT(n_structures_array[0],
              2u);
}

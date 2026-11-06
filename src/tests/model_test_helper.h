/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Feeds a model builder with events, numbering the calls, for the tests of the analysis.
 */
#ifndef TESTS_MODEL_TEST_HELPER_H
#define TESTS_MODEL_TEST_HELPER_H

#include "analysis/api_dump_analysis/model_builder.h"
#include <array>
#include <stdint.h>
#include <vector>

namespace Tests
{
    class ModelTestHelper
    {
    public:
        /* Without history, as a player would run the builder (see ModelBuilder). */
        explicit ModelTestHelper(const bool& in_keeps_history = true);

        /* Renders to the color and the depth resource (either can be NONE) from now on. */
        void bind(const Analysis::APIDumpAnalysis::ResourceId& in_color_id,
                  const Analysis::APIDumpAnalysis::ResourceId& in_depth_id = Analysis::APIDumpAnalysis::ResourceId::NONE);

        void clear(const bool& in_clears_color,
                   const bool& in_clears_depth);
        void clear(const Analysis::APIDumpAnalysis::ClearOperation& in_clear);

        void copy(const Analysis::APIDumpAnalysis::ResourceId& in_source_id,
                  const Analysis::APIDumpAnalysis::ResourceId& in_destination_id);

        /* A 512x512 color texture which can be rendered to and sampled. */
        Analysis::APIDumpAnalysis::ResourceId create_color_texture();

        Analysis::APIDumpAnalysis::ResourceId create_depth_buffer();

        /* A 512x512 texture, uploaded once and sampled. */
        Analysis::APIDumpAnalysis::ResourceId create_static_texture();

        Analysis::APIDumpAnalysis::ResourceId create_swapchain_image();

        Analysis::APIDumpAnalysis::ResourceId create_vertex_buffer();

        void draw(const uint32_t& in_state_key = 0,
                  const uint32_t& in_vertex_count = 3);

        /* A draw whose vertices are known: the positions of a few of them, in the space of its object. */
        void draw_with_geometry(const std::vector<std::array<float, 3> >& in_position_vec,
                                const uint32_t&                           in_vertex_count);

        /* The same, for vertices that are already in the space of the picture: the x and y of a few of them, in pixels, and the reciprocal of the
         * depth. */
        void draw_with_screen_geometry(const std::vector<std::array<float, 3> >& in_position_vec,
                                       const uint32_t&                           in_vertex_count);

        /* A draw which reads what it renders to: it blends, and tests the depth. */
        void draw_reading_attachments();

        /* Draws with the texture in the first binding point. */
        void draw_textured(const Analysis::APIDumpAnalysis::ResourceId& in_texture_id,
                           const uint32_t&                              in_state_key = 0);

        void end_frame();

        /* The match keys of the passes of a frame, in the order they were made. The frame must be one that the model has
         * the detail of.
         */
        std::vector<uint64_t> get_ordered_pass_keys(const uint64_t& in_frame_index) const;

        Analysis::APIDumpAnalysis::ModelBuilder& get_builder();
        const Analysis::APIDumpAnalysis::Model&  get_model  () const;

        /* The call index the next event gets. */
        uint64_t get_next_call_index() const;

        /* The match key of the kind of frame, made of the keys of its passes. */
        uint64_t get_shape_key(const uint64_t& in_frame_index) const;

        /* A matrix of the camera or of the object (the byte offset tells constants apart, as for a fixed-function state). */
        void set_matrix     (const Analysis::APIDumpAnalysis::ConstantSemantic& in_semantic,
                             const uint64_t&                                    in_byte_offset,
                             const std::array<float, 16>&                       in_matrix_f32mat);
        void set_view_matrix(const uint8_t&                                     in_marker);
        void set_viewport   (const float&                                       in_x,
                             const float&                                       in_y,
                             const float&                                       in_width,
                             const float&                                       in_height);

        void upload(const Analysis::APIDumpAnalysis::ResourceId& in_id);

    private:
        Analysis::APIDumpAnalysis::ResourceId create_texture(const bool& in_is_render_target,
                                                             const bool& in_is_depth);

        void begin_call();

        Analysis::APIDumpAnalysis::ModelBuilder m_builder;
        uint64_t                                m_next_call_index;
    };
}

#endif /* TESTS_MODEL_TEST_HELPER_H */

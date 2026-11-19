/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_argument_reader.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_draw_handler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_geometry_sampler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_vertex_reader.h"
#include "common/macros.h"
#include <array>

Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::GLDrawHandler(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_array_draw_describer(in_builder_ptr),
     m_builder_ptr         (in_builder_ptr),
     m_n_unfollowed_calls  (0)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::add_view_anchor(const GLContextState&                       in_state,
                                                                         Analysis::APIDumpAnalysis::DrawDescription* out_description_ptr)
{
    out_description_ptr->view_anchor_call_index = in_state.modelview_anchor_call_index_vec.back();
}

void Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::add_block_vertices(const uint32_t& in_n_vertices,
                                                                              GLContextState* inout_state_ptr)
{
    GLImmediateBlock& block = inout_state_ptr->immediate_block;

    if (block.is_open)
    {
        AI_ASSERT_ALWAYS(block.wants_positions == false,
                         "Vertices that are counted without being given cannot be part of a block whose positions are kept");

        block.n_vertices += in_n_vertices;
    }
}

const Analysis::APIDumpAnalysis::Frontends::GLBlobStore& Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::get_blob_store() const
{
    return m_array_draw_describer.get_blob_store();
}

uint64_t Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::get_n_unfollowed_calls() const
{
    return m_n_unfollowed_calls;
}

Analysis::APIDumpAnalysis::PrimitiveTopology Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::get_primitive_topology(const uint32_t& in_mode)
{
    constexpr uint32_t LINES          = 0x0001;
    constexpr uint32_t LINE_LOOP      = 0x0002;
    constexpr uint32_t LINE_STRIP     = 0x0003;
    constexpr uint32_t POINTS         = 0x0000;
    constexpr uint32_t POLYGON        = 0x0009;
    constexpr uint32_t QUADS          = 0x0007;
    constexpr uint32_t QUAD_STRIP     = 0x0008;
    constexpr uint32_t TRIANGLES      = 0x0004;
    constexpr uint32_t TRIANGLE_FAN   = 0x0006;
    constexpr uint32_t TRIANGLE_STRIP = 0x0005;

    Analysis::APIDumpAnalysis::PrimitiveTopology result = Analysis::APIDumpAnalysis::PrimitiveTopology::UNKNOWN;

    switch (in_mode)
    {
        case LINES:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::LINE_LIST;

            break;
        }

        case LINE_LOOP:
        case LINE_STRIP:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::LINE_STRIP;

            break;
        }

        case POINTS:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::POINT_LIST;

            break;
        }

        case POLYGON:
        case TRIANGLE_FAN:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_FAN;

            break;
        }

        case QUADS:
        case TRIANGLES:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST;

            break;
        }

        case QUAD_STRIP:
        case TRIANGLE_STRIP:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_STRIP;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::handle_array_draw(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                            const Capture::APIDumpLoader::Workload&      in_workload,
                                                                            const bool&                                  in_is_indexed,
                                                                            GLWindow*                                    inout_window_ptr,
                                                                            GLTextureHandler*                            inout_texture_handler_ptr,
                                                                            GLContextState*                              inout_state_ptr)
{
    constexpr size_t MODE_ARGUMENT = 0;

    uint32_t mode         = 0;
    bool     is_mode_read = false;

    if (in_call.arg_in_vec.size() > MODE_ARGUMENT)
    {
        is_mode_read = APIInterceptor::read_number(in_call.arg_in_vec.at(MODE_ARGUMENT),
                                                   &mode);
    }

    if (is_mode_read == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const auto                                 topology = get_primitive_topology(mode);
        Analysis::APIDumpAnalysis::DrawDescription description;

        const auto result = m_array_draw_describer.describe(in_call,
                                                            in_workload,
                                                            in_is_indexed,
                                                            topology,
                                                            inout_state_ptr->pipeline_state,
                                                            &description);

        switch (result)
        {
            case GLArrayDrawResult::DRAW:
            {
                inout_window_ptr->ensure_default_framebuffer(*inout_state_ptr);

                inout_texture_handler_ptr->add_texture_bindings(*inout_state_ptr,
                                                                &description);
                add_view_anchor                                (*inout_state_ptr,
                                                                &description);
                inout_texture_handler_ptr->report_snapshots    (*inout_state_ptr);

                m_builder_ptr->on_draw(description);

                break;
            }

            case GLArrayDrawResult::EMPTY:
            {
                break;
            }

            default:
            {
                m_n_unfollowed_calls++;

                break;
            }
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::handle_begin(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       GLContextState*                              inout_state_ptr)
{
    std::array<uint32_t, 1> mode_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                        &mode_array) )
    {
        GLImmediateBlock& block = inout_state_ptr->immediate_block;

        /* A glBegin inside an open block restarts the block. */
        block.first_call_index = m_builder_ptr->get_call_index();
        block.is_open         = true;
        block.mode            = mode_array.at(0);
        block.n_vertices      = 0;
        block.wants_positions = m_builder_ptr->wants_geometry();

        block.position_vec.clear();
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::handle_clear(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       GLWindow*                                    inout_window_ptr,
                                                                       ResolutionHazardCounter*                     inout_counter_ptr,
                                                                       GLContextState*                              inout_state_ptr)
{
    constexpr uint32_t COLOR_BUFFER_BIT   = 0x4000;
    constexpr uint32_t DEPTH_BUFFER_BIT   = 0x0100;
    constexpr uint32_t STENCIL_BUFFER_BIT = 0x0400;

    std::array<uint32_t, 1> mask_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                        &mask_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        inout_window_ptr->ensure_default_framebuffer(*inout_state_ptr);

        const GLBox& box = inout_state_ptr->attributes.scissor_box;

        /* With the scissor test on, a clear affects only the scissor box, unless the box covers the whole framebuffer. */
        const int32_t framebuffer_height = static_cast<int32_t>(inout_window_ptr->get_framebuffer_height() );
        const int32_t framebuffer_width  = static_cast<int32_t>(inout_window_ptr->get_framebuffer_width() );

        const bool is_box_whole = (box.x      == 0                 &&
                                   box.y      == 0                 &&
                                   box.width  == framebuffer_width &&
                                   box.height == framebuffer_height);

        Analysis::APIDumpAnalysis::ClearOperation clear;

        clear.clears_color           = ( (mask_array.at(0) & COLOR_BUFFER_BIT)   != 0);
        clear.clears_depth           = ( (mask_array.at(0) & DEPTH_BUFFER_BIT)   != 0);
        clear.clears_stencil         = ( (mask_array.at(0) & STENCIL_BUFFER_BIT) != 0);
        clear.color_fp32vec4         = inout_state_ptr->attributes.clear_color_fp32vec4;
        clear.depth                  = static_cast<float>(inout_state_ptr->attributes.clear_depth);
        clear.has_rectangles         = (inout_state_ptr->attributes.is_scissor_enabled &&
                                        is_box_whole == false);
        clear.is_limited_to_viewport = false;
        clear.stencil                = inout_state_ptr->attributes.clear_stencil;

        m_builder_ptr->on_clear(clear);

        if (clear.has_rectangles)
        {
            inout_counter_ptr->count(Analysis::APIDumpAnalysis::ResolutionHazard::CLEARS_WITH_RECTANGLES);
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::handle_end(GLWindow*         inout_window_ptr,
                                                                     GLTextureHandler* inout_texture_handler_ptr,
                                                                     GLContextState*   inout_state_ptr)
{
    GLImmediateBlock& block = inout_state_ptr->immediate_block;

    /* The vertices of a block form one draw. A glEnd without a glBegin, or a block without vertices, produces no draw. */
    if (block.is_open &&
        block.n_vertices > 0)
    {
        inout_window_ptr->ensure_default_framebuffer(*inout_state_ptr);

        const auto topology = get_primitive_topology(block.mode);

        auto description = inout_state_ptr->pipeline_state.describe_draw(topology,
                                                                         block.n_vertices);

        /* The vertex calls belong to the draw. A draw that is left out is left out from its glBegin on. */
        description.first_call_index = block.first_call_index;

        inout_texture_handler_ptr->add_texture_bindings(*inout_state_ptr,
                                                        &description);
        add_view_anchor                                (*inout_state_ptr,
                                                        &description);
        inout_texture_handler_ptr->report_snapshots    (*inout_state_ptr);

        if (block.wants_positions)
        {
            description.geometry_ptr = GLGeometrySampler::make_sample(block.position_vec);
        }

        m_builder_ptr->on_draw(description);
    }

    block.is_open = false;
}

void Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::handle_vertex(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                        const Capture::APIDumpLoader::Workload&      in_workload,
                                                                        GLContextState*                              inout_state_ptr)
{
    GLImmediateBlock& block = inout_state_ptr->immediate_block;

    /* A vertex outside a glBegin/glEnd block is not used by OpenGL. */
    if (block.is_open)
    {
        block.n_vertices++;

        if (block.wants_positions)
        {
            std::array<float, 3> position = {};

            if (GLVertexReader::read_position(in_call,
                                              in_workload,
                                              &position) )
            {
                block.position_vec.push_back(position);
            }
            else
            {
                m_n_unfollowed_calls++;
            }
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLDrawHandler::on_frame_ended()
{
    m_array_draw_describer.on_frame_ended();
}

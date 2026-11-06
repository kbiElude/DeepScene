/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "model_test_helper.h"
#include "analysis/api_dump_analysis/key_builder.h"

Tests::ModelTestHelper::ModelTestHelper(const bool& in_keeps_history)
    :m_builder        (in_keeps_history),
     m_next_call_index(0)
{
    /* Stub */
}

void Tests::ModelTestHelper::begin_call()
{
    constexpr uint64_t FIRST_TIMESTAMP  = 1000;
    constexpr uint64_t TICKS_PER_CALL   = 10;

    m_builder.begin_call(m_next_call_index,
                         FIRST_TIMESTAMP + m_next_call_index * TICKS_PER_CALL);

    m_next_call_index++;
}

void Tests::ModelTestHelper::bind(const Analysis::APIDumpAnalysis::ResourceId& in_color_id,
                                  const Analysis::APIDumpAnalysis::ResourceId& in_depth_id)
{
    Analysis::APIDumpAnalysis::AttachmentSet attachments;

    if (in_color_id != Analysis::APIDumpAnalysis::ResourceId::NONE)
    {
        attachments.color_view_vec.push_back({0, 0, in_color_id});
    }

    attachments.depth_stencil_view.resource_id = in_depth_id;

    begin_call();

    m_builder.on_set_attachments(attachments);
}

void Tests::ModelTestHelper::clear(const bool& in_clears_color,
                                   const bool& in_clears_depth)
{
    Analysis::APIDumpAnalysis::ClearOperation clear_operation;

    clear_operation.clears_color = in_clears_color;
    clear_operation.clears_depth = in_clears_depth;

    begin_call();

    m_builder.on_clear(clear_operation);
}

void Tests::ModelTestHelper::clear(const Analysis::APIDumpAnalysis::ClearOperation& in_clear)
{
    begin_call();

    m_builder.on_clear(in_clear);
}

void Tests::ModelTestHelper::copy(const Analysis::APIDumpAnalysis::ResourceId& in_source_id,
                                  const Analysis::APIDumpAnalysis::ResourceId& in_destination_id)
{
    begin_call();

    m_builder.on_copy({0, 0, in_source_id},
                      {0, 0, in_destination_id});
}

Analysis::APIDumpAnalysis::ResourceId Tests::ModelTestHelper::create_color_texture()
{
    return create_texture(true,   /* in_is_render_target */
                          false); /* in_is_depth         */
}

Analysis::APIDumpAnalysis::ResourceId Tests::ModelTestHelper::create_depth_buffer()
{
    return create_texture(false,  /* in_is_render_target */
                          true);  /* in_is_depth         */
}

Analysis::APIDumpAnalysis::ResourceId Tests::ModelTestHelper::create_static_texture()
{
    return create_texture(false,  /* in_is_render_target */
                          false); /* in_is_depth         */
}

Analysis::APIDumpAnalysis::ResourceId Tests::ModelTestHelper::create_swapchain_image()
{
    constexpr uint32_t HEIGHT                = 600;
    constexpr uint32_t RAW_FORMAT_X8R8G8B8   = 22;
    constexpr uint32_t WIDTH                 = 800;

    Analysis::APIDumpAnalysis::ResourceDescription description;

    description.format.format_class = Analysis::APIDumpAnalysis::FormatClass::COLOR;
    description.format.raw_value    = RAW_FORMAT_X8R8G8B8;
    description.height              = HEIGHT;
    description.is_render_target    = true;
    description.kind                = Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE;
    description.width               = WIDTH;

    begin_call();

    return m_builder.on_create_resource(description,
                                        "swapchain image");
}

Analysis::APIDumpAnalysis::ResourceId Tests::ModelTestHelper::create_texture(const bool& in_is_render_target,
                                                                             const bool& in_is_depth)
{
    constexpr uint32_t EXTENT              = 512;
    constexpr uint32_t RAW_FORMAT_D24S8    = 75;
    constexpr uint32_t RAW_FORMAT_X8R8G8B8 = 22;

    Analysis::APIDumpAnalysis::ResourceDescription description;

    description.format.format_class = (in_is_depth) ? Analysis::APIDumpAnalysis::FormatClass::DEPTH_STENCIL
                                                    : Analysis::APIDumpAnalysis::FormatClass::COLOR;
    description.format.raw_value    = (in_is_depth) ? RAW_FORMAT_D24S8
                                                    : RAW_FORMAT_X8R8G8B8;
    description.height              = EXTENT;
    description.is_depth_stencil    = in_is_depth;
    description.is_render_target    = in_is_render_target;
    description.kind                = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D;
    description.width               = EXTENT;

    begin_call();

    return m_builder.on_create_resource(description,
                                        "texture");
}

Analysis::APIDumpAnalysis::ResourceId Tests::ModelTestHelper::create_vertex_buffer()
{
    constexpr uint64_t BUFFER_N_BYTES = 4096;

    Analysis::APIDumpAnalysis::ResourceDescription description;

    description.is_dynamic    = true;
    description.kind          = Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX;
    description.size_in_bytes = BUFFER_N_BYTES;

    begin_call();

    return m_builder.on_create_resource(description,
                                        "vertex buffer");
}

void Tests::ModelTestHelper::draw(const uint32_t& in_state_key,
                                  const uint32_t& in_vertex_count)
{
    Analysis::APIDumpAnalysis::DrawDescription draw_description;

    draw_description.primitive_topology = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST;
    draw_description.state_key          = in_state_key;
    draw_description.vertex_count       = in_vertex_count;

    begin_call();

    m_builder.on_draw(draw_description);
}

void Tests::ModelTestHelper::draw_reading_attachments()
{
    constexpr uint32_t VERTEX_COUNT = 3;

    Analysis::APIDumpAnalysis::DrawDescription draw_description;

    draw_description.primitive_topology      = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST;
    draw_description.reads_color_attachments = true;
    draw_description.reads_depth_stencil     = true;
    draw_description.vertex_count            = VERTEX_COUNT;

    begin_call();

    m_builder.on_draw(draw_description);
}

void Tests::ModelTestHelper::draw_with_geometry(const std::vector<std::array<float, 3> >& in_position_vec,
                                                const uint32_t&                           in_vertex_count)
{
    auto geometry_ptr = std::make_shared<Analysis::APIDumpAnalysis::GeometrySample>();

    geometry_ptr->position_vec = in_position_vec;

    Analysis::APIDumpAnalysis::DrawDescription draw_description;

    draw_description.geometry_ptr       = geometry_ptr;
    draw_description.primitive_topology = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST;
    draw_description.vertex_count       = in_vertex_count;

    begin_call();

    m_builder.on_draw(draw_description);
}

void Tests::ModelTestHelper::draw_with_screen_geometry(const std::vector<std::array<float, 3> >& in_position_vec,
                                                       const uint32_t&                           in_vertex_count)
{
    auto geometry_ptr = std::make_shared<Analysis::APIDumpAnalysis::GeometrySample>();

    geometry_ptr->is_pre_transformed = true;
    geometry_ptr->position_vec       = in_position_vec;

    Analysis::APIDumpAnalysis::DrawDescription draw_description;

    draw_description.geometry_ptr       = geometry_ptr;
    draw_description.primitive_topology = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST;
    draw_description.vertex_count       = in_vertex_count;

    begin_call();

    m_builder.on_draw(draw_description);
}

void Tests::ModelTestHelper::draw_textured(const Analysis::APIDumpAnalysis::ResourceId& in_texture_id,
                                           const uint32_t&                              in_state_key)
{
    constexpr uint32_t VERTEX_COUNT = 3;

    Analysis::APIDumpAnalysis::DrawDescription draw_description;

    draw_description.primitive_topology = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST;
    draw_description.state_key          = in_state_key;
    draw_description.vertex_count       = VERTEX_COUNT;

    draw_description.texture_binding_vec.push_back({{0, 0, 0}, {0, 0, in_texture_id}});

    begin_call();

    m_builder.on_draw(draw_description);
}

void Tests::ModelTestHelper::end_frame()
{
    begin_call();

    m_builder.on_frame_presented();
}

Analysis::APIDumpAnalysis::ModelBuilder& Tests::ModelTestHelper::get_builder()
{
    return m_builder;
}

const Analysis::APIDumpAnalysis::Model& Tests::ModelTestHelper::get_model() const
{
    return m_builder.get_model();
}

uint64_t Tests::ModelTestHelper::get_next_call_index() const
{
    return m_next_call_index;
}

std::vector<uint64_t> Tests::ModelTestHelper::get_ordered_pass_keys(const uint64_t& in_frame_index) const
{
    const auto&           model          = get_model();
    const auto&           frame_record   = model.frame_record_vec.at(static_cast<size_t>(in_frame_index) );
    std::vector<uint64_t> result_vec;

    for (const auto& entry_ptr : model.representative_store.get_entry_ptr_vec(frame_record.shape_id) )
    {
        if (entry_ptr->frame_index == in_frame_index)
        {
            for (const auto& instance : entry_ptr->detail.pass_instance_vec)
            {
                result_vec.push_back(model.pass_structure_interner.get_structure(instance.structure_id).key);
            }
        }
    }

    return result_vec;
}

uint64_t Tests::ModelTestHelper::get_shape_key(const uint64_t& in_frame_index) const
{
    const auto&           model        = get_model();
    const auto&           frame_record = model.frame_record_vec.at(static_cast<size_t>(in_frame_index) );
    std::vector<uint64_t> pass_key_vec;

    for (const auto& pass_structure_id : model.frame_shape_interner.get_structure(frame_record.shape_id).pass_structure_id_vec)
    {
        pass_key_vec.push_back(model.pass_structure_interner.get_structure(pass_structure_id).key);
    }

    return Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key(pass_key_vec);
}

void Tests::ModelTestHelper::set_matrix(const Analysis::APIDumpAnalysis::ConstantSemantic& in_semantic,
                                        const uint64_t&                                    in_byte_offset,
                                        const std::array<float, 16>&                       in_matrix_f32mat)
{
    Analysis::APIDumpAnalysis::ConstantUpdate update;

    update.location.block_kind  = Analysis::APIDumpAnalysis::ConstantBlockKind::FIXED_FUNCTION_STATE;
    update.location.byte_offset = in_byte_offset;
    update.location.byte_size   = sizeof(in_matrix_f32mat);
    update.provenance           = Analysis::APIDumpAnalysis::ConstantProvenance::API_DEFINED;
    update.semantic             = in_semantic;
    update.value_u8_vec.assign(reinterpret_cast<const uint8_t*>(in_matrix_f32mat.data() ),
                               reinterpret_cast<const uint8_t*>(in_matrix_f32mat.data() ) + sizeof(in_matrix_f32mat) );

    begin_call();

    m_builder.on_update_constant(update);
}

void Tests::ModelTestHelper::set_view_matrix(const uint8_t& in_marker)
{
    constexpr size_t MATRIX_N_BYTES = 64;

    Analysis::APIDumpAnalysis::ConstantUpdate update;

    update.location.block_kind  = Analysis::APIDumpAnalysis::ConstantBlockKind::FIXED_FUNCTION_STATE;
    update.location.byte_offset = MATRIX_N_BYTES;
    update.location.byte_size   = MATRIX_N_BYTES;
    update.provenance           = Analysis::APIDumpAnalysis::ConstantProvenance::API_DEFINED;
    update.semantic             = Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX;
    update.value_u8_vec.assign(MATRIX_N_BYTES,
                               in_marker);

    begin_call();

    m_builder.on_update_constant(update);
}

void Tests::ModelTestHelper::set_viewport(const float& in_x,
                                          const float& in_y,
                                          const float& in_width,
                                          const float& in_height)
{
    Analysis::APIDumpAnalysis::Viewport viewport;

    viewport.height = in_height;
    viewport.width  = in_width;
    viewport.x      = in_x;
    viewport.y      = in_y;

    begin_call();

    m_builder.on_set_viewport(viewport);
}

void Tests::ModelTestHelper::upload(const Analysis::APIDumpAnalysis::ResourceId& in_id)
{
    begin_call();

    m_builder.on_upload({0, 0, in_id},
                        Analysis::APIDumpAnalysis::DataReference() );
}

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/shared/fixed_function_frontend.h"
#include <algorithm>
#include <string.h>

Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::FixedFunctionFrontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_builder_ptr        (in_builder_ptr),
     m_n_unfollowed_calls (0),
     m_n_unsupported_calls(0)
{
    /* Stub */
}

bool Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::find_resource_view(const uint64_t&                          in_address,
                                                                                    Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const
{
    const ObjectTracker::TrackedObject* object_ptr = m_tracker.get_object_ptr(in_address);
    const bool                          result     = (object_ptr != nullptr);

    if (result)
    {
        *out_view_ptr = object_ptr->view;
    }

    return result;
}

const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::get_n_resolution_hazards() const
{
    return m_n_resolution_hazard_map;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::get_n_unfollowed_calls() const
{
    return m_n_unfollowed_calls;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::get_n_unsupported_calls() const
{
    return m_n_unsupported_calls;
}

const Analysis::APIDumpAnalysis::Frontends::ObjectTracker& Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::get_tracker() const
{
    return m_tracker;
}

const Analysis::APIDumpAnalysis::ResourceDescription* Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::find_resource_description_ptr(const Analysis::APIDumpAnalysis::ResourceId& in_id) const
{
    const uint32_t                                        index      = Analysis::APIDumpAnalysis::get_resource_id_value(in_id);
    const Analysis::APIDumpAnalysis::ResourceDescription* result_ptr = nullptr;

    if (index >= 1                                              &&
        index <= m_builder_ptr->get_model().resource_vec.size() )
    {
        result_ptr = &m_builder_ptr->get_model().resource_vec.at(index - 1).description;
    }

    return result_ptr;
}

Analysis::APIDumpAnalysis::Viewport Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::get_full_viewport(const Analysis::APIDumpAnalysis::ResourceView& in_view) const
{
    const Analysis::APIDumpAnalysis::ResourceDescription* description_ptr = find_resource_description_ptr(in_view.resource_id);
    Analysis::APIDumpAnalysis::Viewport                   result;

    if (description_ptr != nullptr)
    {
        /* Each mip level is half the size of the one before, down to one pixel. */
        result.height = static_cast<float>(std::max<uint32_t>(description_ptr->height >> in_view.mip_level,
                                                              1) );
        result.width  = static_cast<float>(std::max<uint32_t>(description_ptr->width  >> in_view.mip_level,
                                                              1) );
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::handle_light_enable(const CallView& in_view)
{
    constexpr size_t ENABLE_ARGUMENT = 2;
    constexpr size_t INDEX_ARGUMENT  = 1;

    const uint32_t light_index = in_view.get_u32(INDEX_ARGUMENT);
    const bool     is_enabled  = (in_view.get_i32(ENABLE_ARGUMENT) != 0);

    if (get_fixed_function_state().set_light_enable(light_index,
                                                    is_enabled) == false)
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::handle_release(const CallView& in_view)
{
    constexpr size_t OBJECT_ARGUMENT = 0;

    const uint64_t                      address    = in_view.get_pointer(OBJECT_ARGUMENT);
    const ObjectTracker::TrackedObject* object_ptr = m_tracker.get_object_ptr(address);

    /* The last reference is gone. */
    if (object_ptr                 != nullptr &&
        in_view.get_returned_u32() == 0)
    {
        if (object_ptr->owns_resource)
        {
            m_builder_ptr->on_destroy_resource(object_ptr->view.resource_id);

            forget_resource(object_ptr->view.resource_id);
        }

        m_tracker.forget(address);
    }

    /* The address may be given to another object: it has no name until it is registered again. */
    if (in_view.get_returned_u32() == 0)
    {
        m_namer.forget_object(address);
    }
}

void Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::handle_set_light(const CallView& in_view)
{
    constexpr size_t INDEX_ARGUMENT = 1;
    constexpr size_t LIGHT_ARGUMENT = 2;

    const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_chunk_ptr(LIGHT_ARGUMENT);

    /* The type is the first field of the light. */
    if (chunk_ptr         == nullptr          ||
        chunk_ptr->size() <  sizeof(uint32_t) )
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const uint32_t light_index = in_view.get_u32(INDEX_ARGUMENT);
        uint32_t       type        = 0;

        ::memcpy(&type,
                 chunk_ptr->data(),
                 sizeof(type) );

        if (get_fixed_function_state().set_light(light_index,
                                                 type) == false)
        {
            m_n_unfollowed_calls++;
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::handle_set_render_state(const CallView& in_view)
{
    constexpr size_t STATE_ARGUMENT = 1;
    constexpr size_t VALUE_ARGUMENT = 2;

    const uint32_t state = in_view.get_u32(STATE_ARGUMENT);
    const uint32_t value = in_view.get_u32(VALUE_ARGUMENT);

    if (get_fixed_function_state().set_render_state(state,
                                                    value) == false)
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::handle_set_texture(const CallView& in_view)
{
    constexpr size_t STAGE_ARGUMENT   = 1;
    constexpr size_t TEXTURE_ARGUMENT = 2;

    const uint64_t                      address    = in_view.get_pointer(TEXTURE_ARGUMENT);
    const ObjectTracker::TrackedObject* object_ptr = m_tracker.get_object_ptr(address);
    const uint32_t                      stage      = in_view.get_u32(STAGE_ARGUMENT);

    if (address == 0)
    {
        /* No texture takes the one in the stage away. */
        if (get_fixed_function_state().set_texture(stage,
                                                   Analysis::APIDumpAnalysis::ResourceView() ) == false)
        {
            m_n_unfollowed_calls++;
        }
    }
    else
    if (object_ptr == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        if (get_fixed_function_state().set_texture(stage,
                                                   object_ptr->view) == false)
        {
            m_n_unfollowed_calls++;
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::handle_set_texture_stage_state(const CallView& in_view)
{
    constexpr size_t STAGE_ARGUMENT = 1;
    constexpr size_t STATE_ARGUMENT = 2;
    constexpr size_t VALUE_ARGUMENT = 3;

    const uint32_t stage = in_view.get_u32(STAGE_ARGUMENT);
    const uint32_t state = in_view.get_u32(STATE_ARGUMENT);
    const uint32_t value = in_view.get_u32(VALUE_ARGUMENT);

    if (get_fixed_function_state().set_texture_stage_state(stage,
                                                           state,
                                                           value) == false)
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::FixedFunctionFrontend::handle_set_transform(const CallView& in_view)
{
    constexpr size_t MATRIX_ARGUMENT = 2;
    constexpr size_t MATRIX_N_BYTES  = 16 * sizeof(float); /* D3DMATRIX, of both APIs */
    constexpr size_t STATE_ARGUMENT  = 1;

    const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_chunk_ptr(MATRIX_ARGUMENT);
    const uint32_t                           state     = in_view.get_u32      (STATE_ARGUMENT);

    if (chunk_ptr         == nullptr       ||
        chunk_ptr->size() != MATRIX_N_BYTES)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (get_transform_semantic(state) != Analysis::APIDumpAnalysis::ConstantSemantic::UNKNOWN)
    {
        Analysis::APIDumpAnalysis::ConstantUpdate& update = m_transform_update_scratch;

        /* The transforms are state of the device, laid out one after the other. */
        update.location.block_kind  = Analysis::APIDumpAnalysis::ConstantBlockKind::FIXED_FUNCTION_STATE;
        update.location.byte_offset = static_cast<uint64_t>(state) * MATRIX_N_BYTES;
        update.location.byte_size   = MATRIX_N_BYTES;
        update.provenance           = Analysis::APIDumpAnalysis::ConstantProvenance::API_DEFINED;
        update.semantic             = get_transform_semantic(state);

        /* The memory of the matrix of the last call is used again. */
        update.value_u8_vec.assign(chunk_ptr->begin(),
                                   chunk_ptr->end() );

        m_builder_ptr->on_update_constant(update);
    }
}

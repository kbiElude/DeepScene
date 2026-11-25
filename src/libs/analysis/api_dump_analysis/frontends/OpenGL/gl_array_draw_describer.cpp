/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_array_draw_describer.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_client_array_snapshot.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_geometry_sampler.h"
#include "analysis/api_dump_analysis/frontends/shared/call_view.h"
#include "analysis/api_dump_analysis/frontends/shared/geometry_sampler.h"
#include "common/argument_reader.h"
#include "utils/hashing/stable_hasher.h"
#include <string.h>

Analysis::APIDumpAnalysis::Frontends::GLArrayDrawDescriber::GLArrayDrawDescriber(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_array_resources(in_builder_ptr),
     m_builder_ptr    (in_builder_ptr)
{
    /* Stub */
}

Analysis::APIDumpAnalysis::Frontends::GLArrayDrawResult Analysis::APIDumpAnalysis::Frontends::GLArrayDrawDescriber::describe(const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                                                                                                             const Capture::APIDumpLoader::Workload&             in_workload,
                                                                                                                             const bool&                                         in_is_indexed,
                                                                                                                             const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                                                                             const GLPipelineState&                              in_pipeline_state,
                                                                                                                             Analysis::APIDumpAnalysis::DrawDescription*         out_description_ptr)
{
    /* glDrawArrays(mode, first, count, arrays) and glDrawElements(mode, count, type, indices, arrays). */
    constexpr size_t ARRAYS_ARGUMENT_OF_ARRAYS   = 3;
    constexpr size_t ARRAYS_ARGUMENT_OF_ELEMENTS = 4;
    constexpr size_t COUNT_ARGUMENT_OF_ARRAYS    = 2;
    constexpr size_t COUNT_ARGUMENT_OF_ELEMENTS  = 1;
    constexpr size_t FIRST_ARGUMENT              = 1;
    constexpr size_t INDICES_ARGUMENT            = 3;
    constexpr size_t TYPE_ARGUMENT               = 2;

    const size_t n_min_arguments = (in_is_indexed) ? ARRAYS_ARGUMENT_OF_ELEMENTS
                                                   : ARRAYS_ARGUMENT_OF_ARRAYS;
    const size_t n_arguments     = in_call.arg_in_vec.size();

    std::vector<GLClientArray>               array_vec;
    int32_t                                  count             = 0;
    int32_t                                  first             = 0;
    const Capture::APIDumpLoader::DataChunk* indices_chunk_ptr = nullptr;
    uint32_t                                 index_type        = 0;
    GLArrayDrawResult                        result            = GLArrayDrawResult::UNREADABLE;

    if (n_arguments >= n_min_arguments)
    {
        const CallView view(in_call,
                            in_workload,
                            n_arguments);
        bool           are_read = false;

        if (in_is_indexed)
        {
            are_read = APIInterceptor::read_number(in_call.arg_in_vec.at(COUNT_ARGUMENT_OF_ELEMENTS),
                                                   &count);

            if (are_read)
            {
                are_read = APIInterceptor::read_number(in_call.arg_in_vec.at(TYPE_ARGUMENT),
                                                       &index_type);
            }

            indices_chunk_ptr = view.get_chunk_ptr(INDICES_ARGUMENT);
        }
        else
        {
            are_read = APIInterceptor::read_number(in_call.arg_in_vec.at(FIRST_ARGUMENT),
                                                   &first);

            if (are_read)
            {
                are_read = APIInterceptor::read_number(in_call.arg_in_vec.at(COUNT_ARGUMENT_OF_ARRAYS),
                                                       &count);
            }
        }

        if (are_read == false)
        {
            /* Not readable. */
        }
        else
        if (count <= 0)
        {
            result = GLArrayDrawResult::EMPTY;
        }
        else
        {
            const size_t   arrays_argument  = (in_is_indexed) ? ARRAYS_ARGUMENT_OF_ELEMENTS
                                                              : ARRAYS_ARGUMENT_OF_ARRAYS;
            const auto*    arrays_chunk_ptr = view.get_chunk_ptr(arrays_argument);
            const uint32_t index_n_bytes    = GLClientArraySnapshot::get_type_n_bytes(index_type);
            const size_t   indices_n_bytes  = static_cast<size_t>(count) * index_n_bytes;

            /* The call holds no arrays if none of its arrays reads application memory (the draw has no vertex data to see). */
            bool is_valid = true;

            if (arrays_chunk_ptr != nullptr)
            {
                is_valid = GLClientArraySnapshot::parse(*arrays_chunk_ptr,
                                                        &array_vec);
            }

            if (is_valid &&
                in_is_indexed)
            {
                const bool is_index_type = (index_n_bytes == sizeof(uint8_t)  ||
                                            index_n_bytes == sizeof(uint16_t) ||
                                            index_n_bytes == sizeof(uint32_t) );

                is_valid = (is_index_type       &&
                            indices_chunk_ptr   != nullptr);

                if (is_valid)
                {
                    is_valid = (indices_chunk_ptr->size() >= indices_n_bytes);
                }
            }

            if (is_valid)
            {
                Utils::Hashing::StableHasher layout_hasher;

                *out_description_ptr = in_pipeline_state.describe_draw(in_topology,
                                                                       static_cast<uint32_t>(count) );

                out_description_ptr->is_indexed = in_is_indexed;

                for (const auto& array : array_vec)
                {
                    layout_hasher.add_u32(static_cast<uint32_t>(array.kind) );
                    layout_hasher.add_u32(array.index);
                    layout_hasher.add_u32(static_cast<uint32_t>(array.size) );
                    layout_hasher.add_u32(array.type);
                    layout_hasher.add_u32(array.stride);
                    layout_hasher.add_u32(array.flags);
                    layout_hasher.add_u32(array.divisor);

                    if (array.n_bytes > 0)
                    {
                        const auto buffer_id = m_array_resources.get_resource_id(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX,
                                                                                 array.data_ptr,
                                                                                 array.n_bytes);

                        out_description_ptr->vertex_buffer_id_vec.push_back(buffer_id);
                    }
                }

                if (in_is_indexed)
                {
                    layout_hasher.add_u32(index_type);

                    out_description_ptr->index_buffer_id = m_array_resources.get_resource_id(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_INDEX,
                                                                                             indices_chunk_ptr->data(),
                                                                                             indices_n_bytes);
                }

                out_description_ptr->vertex_layout_key = layout_hasher.get_digest();

                if (m_builder_ptr->wants_geometry() )
                {
                    out_description_ptr->geometry_ptr = make_geometry_sample(array_vec,
                                                                             indices_chunk_ptr,
                                                                             index_type,
                                                                             first,
                                                                             count);
                }

                result = GLArrayDrawResult::DRAW;
            }
        }
    }

    return result;
}

std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> Analysis::APIDumpAnalysis::Frontends::GLArrayDrawDescriber::make_geometry_sample(const std::vector<GLClientArray>&        in_array_vec,
                                                                                                                                                  const Capture::APIDumpLoader::DataChunk* in_indices_chunk_ptr,
                                                                                                                                                  const uint32_t&                          in_index_type,
                                                                                                                                                  const int32_t&                           in_first,
                                                                                                                                                  const int32_t&                           in_count) const
{
    const GLClientArray*                                       position_array_ptr = nullptr;
    std::shared_ptr<Analysis::APIDumpAnalysis::GeometrySample> result_ptr;

    for (const auto& array : in_array_vec)
    {
        if (array.kind  == GLClientArrayKind::VERTEX &&
            array.index == 0)
        {
            position_array_ptr = &array;

            break;
        }
    }

    if (position_array_ptr != nullptr)
    {
        const size_t n_vertices = static_cast<size_t>(in_count);
        const size_t n_samples  = GeometrySampler::get_n_samples(n_vertices);

        result_ptr = std::make_shared<Analysis::APIDumpAnalysis::GeometrySample>();

        for (size_t n_sample = 0;
                    n_sample < n_samples;
                  ++n_sample)
        {
            const size_t         place    = GeometrySampler::get_sample_vertex(n_sample,
                                                                               n_vertices);
            uint32_t             element  = static_cast<uint32_t>(in_first) + static_cast<uint32_t>(place);
            std::array<float, 3> position = {};

            if (in_indices_chunk_ptr != nullptr)
            {
                element = read_index(*in_indices_chunk_ptr,
                                     in_index_type,
                                     place);
            }

            const bool is_read = GLClientArraySnapshot::read_position(*position_array_ptr,
                                                                      element,
                                                                      &position);

            if (is_read)
            {
                result_ptr->position_vec.push_back(position);
            }
        }

        if (result_ptr->position_vec.empty() )
        {
            result_ptr.reset();
        }
    }

    return result_ptr;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::GLArrayDrawDescriber::read_index(const Capture::APIDumpLoader::DataChunk& in_indices_chunk,
                                                                                const uint32_t&                          in_index_type,
                                                                                const size_t&                            in_place)
{
    const uint32_t index_n_bytes = GLClientArraySnapshot::get_type_n_bytes(in_index_type);
    uint32_t       result        = 0;

    ::memcpy(&result,
             in_indices_chunk.data() + in_place * index_n_bytes,
             index_n_bytes);

    return result;
}

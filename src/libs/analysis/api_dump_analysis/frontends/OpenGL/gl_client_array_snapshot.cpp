/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_client_array_snapshot.h"
#include <string.h>

uint32_t Analysis::APIDumpAnalysis::Frontends::GlClientArraySnapshot::get_element_stride(const GlClientArray& in_array)
{
    uint32_t result = in_array.stride;

    /* Elements which follow each other fill the part of the array that was copied. */
    if (result              == 0 &&
        in_array.n_elements != 0)
    {
        result = in_array.n_bytes / in_array.n_elements;
    }

    return result;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::GlClientArraySnapshot::get_type_n_bytes(const uint32_t& in_type)
{
    uint32_t result = 0;

    switch (in_type)
    {
        case TYPE_BYTE:
        case TYPE_UNSIGNED_BYTE:
        {
            result = sizeof(uint8_t);

            break;
        }

        case TYPE_DOUBLE:
        {
            result = sizeof(double);

            break;
        }

        case TYPE_FLOAT:
        {
            result = sizeof(float);

            break;
        }

        case TYPE_INT:
        case TYPE_UNSIGNED_INT:
        {
            result = sizeof(uint32_t);

            break;
        }

        case TYPE_SHORT:
        case TYPE_UNSIGNED_SHORT:
        {
            result = sizeof(uint16_t);

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GlClientArraySnapshot::parse(const Capture::APIDumpLoader::DataChunk& in_chunk,
                                                                        std::vector<GlClientArray>*              out_array_vec_ptr)
{
    const uint8_t* chunk_ptr = in_chunk.data();
    const size_t   n_bytes   = in_chunk.size();
    uint32_t       n_arrays  = 0;
    size_t         offset    = sizeof(n_arrays);
    bool           result    = (n_bytes >= sizeof(n_arrays) );

    out_array_vec_ptr->clear();

    if (result)
    {
        ::memcpy(&n_arrays,
                 chunk_ptr,
                 sizeof(n_arrays) );
    }

    for (uint32_t n_array = 0;
                  result &&
                  n_array < n_arrays;
                ++n_array)
    {
        PackedArray packed = {};

        result = (offset + sizeof(packed) <= n_bytes);

        if (result)
        {
            ::memcpy(&packed,
                     chunk_ptr + offset,
                     sizeof(packed) );

            offset += sizeof(packed);

            result = (offset + packed.n_bytes <= n_bytes);
        }

        if (result)
        {
            GlClientArray array;

            array.data_ptr      = chunk_ptr + offset;
            array.divisor       = packed.divisor;
            array.first_element = packed.first_element;
            array.flags         = packed.flags;
            array.index         = packed.index;
            array.kind          = static_cast<GlClientArrayKind>(packed.kind);
            array.n_bytes       = packed.n_bytes;
            array.n_elements    = packed.n_elements;
            array.pointer       = packed.pointer;
            array.size          = packed.size;
            array.stride        = packed.stride;
            array.type          = packed.type;

            out_array_vec_ptr->push_back(array);

            offset += packed.n_bytes;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GlClientArraySnapshot::read_component(const uint32_t& in_type,
                                                                                 const uint8_t*  in_data_ptr,
                                                                                 double*         out_value_ptr)
{
    bool result = true;

    switch (in_type)
    {
        case TYPE_DOUBLE:
        {
            double value = 0.0;

            ::memcpy(&value,
                     in_data_ptr,
                     sizeof(value) );

            *out_value_ptr = value;

            break;
        }

        case TYPE_FLOAT:
        {
            float value = 0.0f;

            ::memcpy(&value,
                     in_data_ptr,
                     sizeof(value) );

            *out_value_ptr = static_cast<double>(value);

            break;
        }

        case TYPE_INT:
        {
            int32_t value = 0;

            ::memcpy(&value,
                     in_data_ptr,
                     sizeof(value) );

            *out_value_ptr = static_cast<double>(value);

            break;
        }

        case TYPE_SHORT:
        {
            int16_t value = 0;

            ::memcpy(&value,
                     in_data_ptr,
                     sizeof(value) );

            *out_value_ptr = static_cast<double>(value);

            break;
        }

        default:
        {
            result = false;

            break;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GlClientArraySnapshot::read_position(const GlClientArray&  in_array,
                                                                                const uint32_t&       in_element,
                                                                                std::array<float, 3>* out_position_ptr)
{
    constexpr int32_t MAX_N_COMPONENTS = 3;

    const uint32_t component_n_bytes = get_type_n_bytes(in_array.type);
    const uint32_t element_stride    = get_element_stride(in_array);
    const int32_t  n_components      = (in_array.size < MAX_N_COMPONENTS) ? in_array.size
                                                                          : MAX_N_COMPONENTS;
    bool           result            = (in_element        >= in_array.first_element                       &&
                                        in_element        <  in_array.first_element + in_array.n_elements &&
                                        n_components      >  0                                            &&
                                        component_n_bytes >  0);
    uint64_t       element_offset    = 0;

    if (result)
    {
        element_offset = static_cast<uint64_t>(in_element - in_array.first_element) * element_stride;
        result         = (element_offset + static_cast<uint64_t>(n_components) * component_n_bytes <= in_array.n_bytes);
    }

    *out_position_ptr = {};

    for (int32_t n_component = 0;
                 result      &&
                 n_component < n_components;
               ++n_component)
    {
        const uint64_t component_offset = element_offset + static_cast<uint64_t>(n_component) * component_n_bytes;
        double         value            = 0.0;

        result = read_component(in_array.type,
                                in_array.data_ptr + component_offset,
                                &value);

        out_position_ptr->at(static_cast<size_t>(n_component) ) = static_cast<float>(value);
    }

    return result;
}

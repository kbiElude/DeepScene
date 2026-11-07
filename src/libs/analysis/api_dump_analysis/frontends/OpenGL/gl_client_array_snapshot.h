/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the snapshot of the vertex arrays that the dumper makes at a draw (it is the last argument of glDrawArrays and glDrawElements; the
 * layout is described with ClientArrayManager in the interception), and the elements of an array.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_CLIENT_ARRAY_SNAPSHOT_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_CLIENT_ARRAY_SNAPSHOT_H

#include "analysis/api_dump_analysis/frontends/OpenGL/types.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include <array>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GlClientArraySnapshot
            {
            public:
                /* The distance between the elements of the array, in bytes. */
                static uint32_t get_element_stride(const GlClientArray& in_array);

                /* The size of a GL type of a component (a byte, a short, an int, a float or a double), in bytes; 0 for any other. */
                static uint32_t get_type_n_bytes(const uint32_t& in_type);

                /* The arrays of the snapshot, which point into its data; false if it is not the size that it says it is. */
                static bool parse(const Capture::APIDumpLoader::DataChunk& in_chunk,
                                  std::vector<GlClientArray>*              out_array_vec_ptr);

                /* The first three components of an element as numbers (the third is 0 if the array has two); false if the element is not
                 * in the part of the array that the snapshot has, or the type of a component is not a number that a position is made of.
                 */
                static bool read_position(const GlClientArray&  in_array,
                                          const uint32_t&       in_element,
                                          std::array<float, 3>* out_position_ptr);

            private:
                /* The GL types of a component. */
                enum : uint32_t
                {
                    TYPE_BYTE           = 0x1400,
                    TYPE_DOUBLE         = 0x140A,
                    TYPE_FLOAT          = 0x1406,
                    TYPE_INT            = 0x1404,
                    TYPE_SHORT          = 0x1402,
                    TYPE_UNSIGNED_BYTE  = 0x1401,
                    TYPE_UNSIGNED_INT   = 0x1405,
                    TYPE_UNSIGNED_SHORT = 0x1403
                };

                /* One array as the dumper writes it. */
#pragma pack(push, 1)
                struct PackedArray
                {
                    uint32_t kind;
                    uint32_t index;
                    int32_t  size;
                    uint32_t type;
                    uint32_t stride;
                    uint32_t flags;
                    uint32_t divisor;
                    uint64_t pointer;
                    uint32_t first_element;
                    uint32_t n_elements;
                    uint32_t n_bytes;
                };
#pragma pack(pop)

                static_assert(sizeof(PackedArray) == 48,
                              "Unexpected layout of an array of a snapshot");

                /* A component as a number; false for a type that is not one of the five. */
                static bool read_component(const uint32_t& in_type,
                                           const uint8_t*  in_data_ptr,
                                           double*         out_value_ptr);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_CLIENT_ARRAY_SNAPSHOT_H */

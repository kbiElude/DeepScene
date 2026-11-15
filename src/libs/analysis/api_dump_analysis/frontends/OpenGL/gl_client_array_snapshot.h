/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the snapshot of the vertex arrays that the dumper takes at a draw, and the elements of an array. The snapshot is the last argument of
 * glDrawArrays and glDrawElements. Its layout is described with ClientArrayManager in the interception.
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
            class GLClientArraySnapshot
            {
            public:
                /* The distance between the elements of the array, in bytes. */
                static uint32_t get_element_stride(const GLClientArray& in_array);

                /* The size in bytes of a GL component type (byte, short, int, float or double); 0 for any other type. */
                static uint32_t get_type_n_bytes(const uint32_t& in_type);

                /* Reads the arrays of the snapshot. They point into its data. Returns false if the snapshot is not as large as it declares. */
                static bool parse(const Capture::APIDumpLoader::DataChunk& in_chunk,
                                  std::vector<GLClientArray>*              out_array_vec_ptr);

                /* Reads the first three components of an element as numbers (the third is 0 if the array has two components). Returns false if
                 * the element is outside the part of the array that the snapshot holds, or if the component type cannot form a position.
                 */
                static bool read_position(const GLClientArray&  in_array,
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

                /* One array as the dumper stores it. */
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

                /* Reads a component as a number. Returns false for a type other than the five supported ones. */
                static bool read_component(const uint32_t& in_type,
                                           const uint8_t*  in_data_ptr,
                                           double*         out_value_ptr);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_CLIENT_ARRAY_SNAPSHOT_H */

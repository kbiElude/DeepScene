/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the position given by a glVertex call. The position has 2, 3 or 4 components, each a double, a float, an int or a short. The values come
 * from the call arguments or, for the pointer forms, from the data that the dumper stored in place of the pointer.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_VERTEX_READER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_VERTEX_READER_H

#include "analysis/api_dump_analysis/frontends/OpenGL/types.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include <array>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLVertexReader
            {
            public:
                /* Describes how the function passes its vertex. Returns false if the function is not a glVertex call. */
                static bool find_layout(const APIInterceptor::APIFunction& in_function,
                                        GLVertexLayout*                    out_layout_ptr);

                /* Reads the x, y and z of the vertex. A z that the call does not give is 0, and w is ignored. Returns false if the call does not
                 * match its function (a data chunk of a different size, or an argument that is not a number).
                 */
                static bool read_position(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const Capture::APIDumpLoader::Workload&      in_workload,
                                          std::array<float, 3>*                        out_position_ptr);

            private:
                /* Reads the first components of the data, using the component kind to interpret the bytes. Returns false if there are too few bytes. */
                static bool read_components(const Capture::APIDumpLoader::DataChunk& in_chunk,
                                            const GLVertexLayout&                    in_layout,
                                            std::array<double, 4>*                   out_component_array_ptr);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_VERTEX_READER_H */

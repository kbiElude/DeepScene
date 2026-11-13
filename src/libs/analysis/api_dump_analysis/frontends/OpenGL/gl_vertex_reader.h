/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the position that a call of glVertex gives: of 2, 3 or 4 components, each a double, a float, an int or a short, as the arguments
 * of the call or (for the calls of a pointer) as the data stored in place of it.
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
                /* How the vertex of the function is given; false if it is not a call of glVertex. */
                static bool find_layout(const APIInterceptor::APIFunction& in_function,
                                        GLVertexLayout*                    out_layout_ptr);

                /* The x, y and z of the vertex (a z that the call does not give is 0, and a w is left out); false if the call has not got what its
                 * function says (a data chunk of another size, an argument that is not a number).
                 */
                static bool read_position(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const Capture::APIDumpLoader::Workload&      in_workload,
                                          std::array<float, 3>*                        out_position_ptr);

            private:
                /* The first components of the data, as the kind of a component says they are kept; false if there are not that many bytes. */
                static bool read_components(const Capture::APIDumpLoader::DataChunk& in_chunk,
                                            const GLVertexLayout&                    in_layout,
                                            std::array<double, 4>*                   out_component_array_ptr);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_VERTEX_READER_H */

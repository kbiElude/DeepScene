/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_ARGUMENT_READER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_ARGUMENT_READER_H

#include "capture/api_dump_loader/api_dump_loader.h"
#include "common/argument_reader.h"
#include <array>
#include <stddef.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* How the classes that follow the calls of OpenGL read the arguments of a call. */
            class GlArgumentReader
            {
            public:
                /* Reads the first N arguments of the call as numbers; false if it has fewer, or one that is not a number. */
                template<typename T, size_t N>
                static bool read_arguments(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           std::array<T, N>*                            out_value_array_ptr)
                {
                    bool result = (in_call.arg_in_vec.size() >= N);

                    for (size_t n_value = 0;
                                result   &&
                                n_value < N;
                              ++n_value)
                    {
                        result = APIInterceptor::read_number(in_call.arg_in_vec.at(n_value),
                                                             &out_value_array_ptr->at(n_value) );
                    }

                    return result;
                }
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_ARGUMENT_READER_H */

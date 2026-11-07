/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Makes the calls of a small OpenGL application (GL, WGL and SwapBuffers), as the dumper would have recorded them, for the tests of the
 * front end.
 */
#ifndef TESTS_GL_CALL_BUILDER_H
#define TESTS_GL_CALL_BUILDER_H

#include "capture/api_dump_loader/api_dump_loader.h"
#include "common/api_function_argument.h"
#include <stdint.h>
#include <vector>

namespace Tests
{
    class GlCallBuilder
    {
    public:
        typedef std::vector<APIInterceptor::APIFunctionArgument> ArgumentVector;

        GlCallBuilder();

        /* The calls are those of the thread that the builder is told to be (1 at first). */
        void add_call(const APIInterceptor::APIFunction&         in_function,
                      const ArgumentVector&                      in_arguments,
                      const APIInterceptor::APIFunctionArgument& in_returned_value = APIInterceptor::APIFunctionArgument::create_i32(0) );

        void begin        (const uint32_t& in_mode);
        void clear        (const uint32_t& in_mask);
        void clear_color  (const float&    in_red,
                           const float&    in_green,
                           const float&    in_blue,
                           const float&    in_alpha);
        void clear_depth  (const double&   in_depth);
        void clear_stencil(const int32_t&  in_stencil);
        void disable      (const uint32_t& in_capability);
        void enable       (const uint32_t& in_capability);

        const Capture::APIDumpLoader::Workload& get_workload() const;

        /* A context address of 0 is an unbind. A call which does not succeed returns FALSE. */
        void make_current(const uint64_t& in_device_context_address,
                          const uint64_t& in_context_address,
                          const bool&     in_succeeds = true);

        void scissor(const int32_t& in_x,
                     const int32_t& in_y,
                     const int32_t& in_width,
                     const int32_t& in_height);

        void set_thread_id(const uint32_t& in_thread_id);

        void swap_buffers(const uint64_t& in_device_context_address);

        void viewport(const int32_t& in_x,
                      const int32_t& in_y,
                      const int32_t& in_width,
                      const int32_t& in_height);

    private:
        static APIInterceptor::APIFunctionArgument make_pointer_argument(const uint64_t& in_address);

        uint64_t                         m_next_timestamp;
        uint32_t                         m_thread_id;
        Capture::APIDumpLoader::Workload m_workload;
    };
}

#endif /* TESTS_GL_CALL_BUILDER_H */

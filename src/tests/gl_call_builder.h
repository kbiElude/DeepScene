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
#include <array>
#include <stddef.h>
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
        void blend_func   (const uint32_t& in_source,
                           const uint32_t& in_destination);
        void clear        (const uint32_t& in_mask);
        void clear_color  (const float&    in_red,
                           const float&    in_green,
                           const float&    in_blue,
                           const float&    in_alpha);
        void clear_depth  (const double&   in_depth);
        void clear_stencil(const int32_t&  in_stencil);
        void depth_func   (const uint32_t& in_function);
        void depth_mask   (const bool&     in_is_writing);
        void disable      (const uint32_t& in_capability);
        void enable       (const uint32_t& in_capability);
        void end          ();
        void end_list     ();

        void frustum(const double& in_left,
                     const double& in_right,
                     const double& in_bottom,
                     const double& in_top,
                     const double& in_near,
                     const double& in_far);

        const Capture::APIDumpLoader::Workload& get_workload() const;

        void load_identity();

        /* The matrices are stored as data chunks, as the dumper stores the argument of glLoadMatrix and glMultMatrix. */
        void load_matrix       (const std::array<float, 16>&  in_matrix_f32mat);
        void load_matrix_double(const std::array<double, 16>& in_matrix_d64mat);

        /* A context address of 0 is an unbind. A call which does not succeed returns FALSE. */
        void make_current(const uint64_t& in_device_context_address,
                          const uint64_t& in_context_address,
                          const bool&     in_succeeds = true);

        void matrix_mode(const uint32_t& in_mode);

        void mult_matrix(const std::array<float, 16>& in_matrix_f32mat);

        /* GL_COMPILE is the mode that records the calls and does not make them. */
        void new_list(const uint32_t& in_list,
                      const uint32_t& in_mode);

        void ortho(const double& in_left,
                   const double& in_right,
                   const double& in_bottom,
                   const double& in_top,
                   const double& in_near,
                   const double& in_far);

        void pop_attrib ();
        void pop_matrix ();
        void push_attrib(const uint32_t& in_mask);
        void push_matrix();

        /* glRotatef, glScalef and glTranslated: a float one and a double one of each are the same to the front end. */
        void rotate(const float& in_angle_degrees,
                    const float& in_x,
                    const float& in_y,
                    const float& in_z);

        void scale(const float& in_x,
                   const float& in_y,
                   const float& in_z);

        void scissor(const int32_t& in_x,
                     const int32_t& in_y,
                     const int32_t& in_width,
                     const int32_t& in_height);

        void set_thread_id(const uint32_t& in_thread_id);

        void swap_buffers(const uint64_t& in_device_context_address);

        void translate(const double& in_x,
                       const double& in_y,
                       const double& in_z);

        /* A vertex of two or three floats as arguments, and one of three as the data of a pointer. */
        void vertex2f (const float& in_x,
                       const float& in_y);
        void vertex3f (const float& in_x,
                       const float& in_y,
                       const float& in_z);
        void vertex3fv(const std::array<float, 3>& in_position);

        void viewport(const int32_t& in_x,
                      const int32_t& in_y,
                      const int32_t& in_width,
                      const int32_t& in_height);

    private:
        /* A call whose only argument is the data chunk made of the bytes. */
        void add_chunk_call(const APIInterceptor::APIFunction& in_function,
                            const void*                        in_data_ptr,
                            const size_t&                      in_n_bytes);

        static APIInterceptor::APIFunctionArgument make_pointer_argument(const uint64_t& in_address);

        uint64_t                         m_next_timestamp;
        uint32_t                         m_thread_id;
        Capture::APIDumpLoader::Workload m_workload;
    };
}

#endif /* TESTS_GL_CALL_BUILDER_H */

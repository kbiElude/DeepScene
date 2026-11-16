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
    class GLCallBuilder
    {
    public:
        typedef std::vector<APIInterceptor::APIFunctionArgument> ArgumentVector;

        /* An array that a draw reads from the memory of the application, as the snapshot of the dumper has it. The data starts at the element
         * first_element; a stride of 0 means that the elements follow each other.
         */
        struct ClientArray
        {
            std::vector<uint8_t> data_u8vec;
            uint32_t             first_element = 0;
            uint32_t             kind          = 8;      /* the vertices      */
            uint32_t             n_elements    = 0;
            int32_t              size          = 3;
            uint32_t             stride        = 0;
            uint32_t             type          = 0x1406; /* GL_FLOAT          */
        };

        GLCallBuilder();

        /* The unit is counted from 0. */
        void active_texture(const uint32_t& in_unit);

        /* The calls are those of the thread that the builder is told to be (1 at first). */
        void add_call(const APIInterceptor::APIFunction&         in_function,
                      const ArgumentVector&                      in_arguments,
                      const APIInterceptor::APIFunctionArgument& in_returned_value = APIInterceptor::APIFunctionArgument::create_i32(0) );

        void begin        (const uint32_t& in_mode);
        void bind_buffer  (const uint32_t& in_target,
                           const uint32_t& in_buffer);
        void bind_texture (const uint32_t& in_target,
                           const uint32_t& in_name);
        void blend_func   (const uint32_t& in_source,
                           const uint32_t& in_destination);
        void call_list    (const uint32_t& in_list);
        void clear        (const uint32_t& in_mask);
        void clear_color  (const float&    in_red,
                           const float&    in_green,
                           const float&    in_blue,
                           const float&    in_alpha);
        void clear_depth  (const double&   in_depth);
        void clear_stencil(const int32_t&  in_stencil);
        /* The pixels of the window copied to the bound texture (the first gives the texture its size). */
        void copy_tex_image_2d    (const uint32_t& in_level,
                                   const uint32_t& in_internal_format,
                                   const int32_t&  in_x,
                                   const int32_t&  in_y,
                                   const int32_t&  in_width,
                                   const int32_t&  in_height);
        void copy_tex_sub_image_2d(const uint32_t& in_level,
                                   const int32_t&  in_x_offset,
                                   const int32_t&  in_y_offset,
                                   const int32_t&  in_x,
                                   const int32_t&  in_y,
                                   const int32_t&  in_width,
                                   const int32_t&  in_height);

        /* CreateWindowExA of a window: the rectangle is the outer one, the class and the menu are none. */
        void create_window(const uint64_t& in_window_address,
                           const uint32_t& in_style,
                           const uint32_t& in_ex_style,
                           const int32_t&  in_width,
                           const int32_t&  in_height);

        void delete_lists (const int32_t&  in_first_list,
                           const int32_t&  in_range);
        void delete_textures(const std::vector<uint32_t>& in_name_u32vec);
        void depth_func   (const uint32_t& in_function);
        void depth_mask   (const bool&     in_is_writing);
        void disable      (const uint32_t& in_capability);

        /* The arrays are what the draw reads, and are stored in the call. */
        void draw_arrays  (const uint32_t&                 in_mode,
                           const int32_t&                  in_first,
                           const int32_t&                  in_count,
                           const std::vector<ClientArray>& in_array_vec);
        void draw_elements(const uint32_t&                 in_mode,
                           const std::vector<uint16_t>&    in_index_u16vec,
                           const std::vector<ClientArray>& in_array_vec);

        void enable       (const uint32_t& in_capability);
        void end          ();
        void end_list     ();

        void frustum(const double& in_left,
                     const double& in_right,
                     const double& in_bottom,
                     const double& in_top,
                     const double& in_near,
                     const double& in_far);

        /* glGenerateMipmap. */
        void generate_mipmap(const uint32_t& in_target);

        /* GetDC of a window. */
        void get_dc(const uint64_t& in_window_address,
                    const uint64_t& in_device_context_address);

        const Capture::APIDumpLoader::Workload& get_workload() const;

        void line_width(const float& in_width);

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

        /* glPixelStorei. */
        void pixel_store(const uint32_t& in_parameter,
                         const int32_t&  in_value);

        void point_size(const float& in_size);

        void pop_attrib ();
        void pop_matrix ();
        void push_attrib(const uint32_t& in_mask);
        void push_matrix();

        void read_pixels(const int32_t& in_x,
                         const int32_t& in_y,
                         const int32_t& in_width,
                         const int32_t& in_height);

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

        /* A glTexImage2D of RGBA bytes: with the pixels in the call, or with none (a null pointer) if the vector is empty, or, if it is empty and the last argument is
         * true, with a pointer to pixels and a chunk of no bytes for them (what the dumper made when it did not store them).
         */
        void tex_image_2d(const uint32_t&             in_level,
                          const int32_t&              in_width,
                          const int32_t&              in_height,
                          const std::vector<uint8_t>& in_pixel_u8vec,
                          const bool&                 in_has_empty_chunk = false);

        /* glTexParameteri of the target GL_TEXTURE_2D, or of the target that is given. */
        void tex_parameter(const uint32_t& in_parameter,
                           const int32_t&  in_value,
                           const uint32_t& in_target = 0x0DE1);

        /* A glTexSubImage2D of RGBA bytes: with the pixels in the call, or with none if the vector is empty. */
        void tex_sub_image_2d(const uint32_t&             in_level,
                              const int32_t&              in_x_offset,
                              const int32_t&              in_y_offset,
                              const int32_t&              in_width,
                              const int32_t&              in_height,
                              const std::vector<uint8_t>& in_pixel_u8vec);

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

        /* An array of vertices of three floats each. */
        /* The snapshot of the arrays, as the dumper stores it. */
        static std::vector<uint8_t> make_snapshot(const std::vector<ClientArray>& in_array_vec);

        static ClientArray make_vertex_array(const std::vector<std::array<float, 3> >& in_position_vec);

    private:
        /* The data as a chunk of the workload; the argument that stands for it. */
        APIInterceptor::APIFunctionArgument add_chunk(const void*   in_data_ptr,
                                                      const size_t& in_n_bytes);

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

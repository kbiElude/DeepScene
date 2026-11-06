/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Builds D3D8 call streams for the tests, laid out the way the dumper stores them: pointers to input structures are
 * replaced by data chunks, objects written by a call are in its out arguments.
 */
#ifndef TESTS_D3D8_CALL_BUILDER_H
#define TESTS_D3D8_CALL_BUILDER_H

#include "capture/api_dump_loader/api_dump_loader.h"
#include "common/api_function_argument.h"
#include "interception/hooks/D3D8/types.h"
#include <stdint.h>
#include <vector>

namespace Tests
{
    class D3D8CallBuilder
    {
    public:
        typedef std::vector<APIInterceptor::APIFunctionArgument> ArgumentVector;

        D3D8CallBuilder();

        /* The out arguments default to the in arguments. The returned value is that of a HRESULT-returning method. */
        void add_call(const APIInterceptor::APIFunction&         in_function,
                      const ArgumentVector&                      in_arguments,
                      const ArgumentVector&                      in_out_arguments = {},
                      const APIInterceptor::APIFunctionArgument& in_returned_value = APIInterceptor::APIFunctionArgument::create_i32(0));

        /* The address which the next object will have, as the dumped process would see it. */
        uint64_t allocate_address();

        void begin_scene      ();
        void begin_state_block();
        void clear            (const uint32_t& in_flags,
                               const uint32_t& in_color,
                               const float&    in_depth,
                               const uint32_t& in_stencil);
        void copy_rects       (const uint64_t& in_source_address,
                               const uint64_t& in_destination_address);

        /* The create_* functions return the address of the new object. A non-zero address is used instead of a new one;
         * that is how a test makes an address reused after the object has been released.
         */
        uint64_t create_depth_stencil_surface(const uint32_t&                             in_width,
                                              const uint32_t&                             in_height,
                                              const Interception::Hooks::D3D8::D3DFORMAT& in_format,
                                              const uint64_t&                             in_address = 0);

        uint64_t create_device         ();
        uint64_t create_index_buffer   (const uint32_t&                                    in_length,
                                        const uint64_t&                                    in_address = 0);
        uint32_t create_pixel_shader   (const std::vector<uint8_t>&                        in_function_u8_vec);
        uint64_t create_texture        (const uint32_t&                                    in_width,
                                        const uint32_t&                                    in_height,
                                        const uint32_t&                                    in_n_levels,
                                        const uint32_t&                                    in_usage,
                                        const Interception::Hooks::D3D8::D3DFORMAT&        in_format,
                                        const uint64_t&                                    in_address = 0);
        uint64_t create_vertex_buffer  (const uint32_t&                                    in_length,
                                        const uint32_t&                                    in_fvf,
                                        const uint64_t&                                    in_address = 0);
        uint32_t create_vertex_shader  (const std::vector<uint8_t>&                        in_declaration_u8_vec,
                                        const std::vector<uint8_t>&                        in_function_u8_vec);
        void     delete_pixel_shader   (const uint32_t&                                    in_handle);
        void     delete_vertex_shader  (const uint32_t&                                    in_handle);
        void     draw_indexed_primitive(const Interception::Hooks::D3D8::D3DPRIMITIVETYPE& in_type,
                                        const uint32_t&                                    in_min_index,
                                        const uint32_t&                                    in_n_vertices,
                                        const uint32_t&                                    in_start_index,
                                        const uint32_t&                                    in_n_primitives);
        void     draw_primitive        (const Interception::Hooks::D3D8::D3DPRIMITIVETYPE& in_type,
                                        const uint32_t&                                    in_start_vertex,
                                        const uint32_t&                                    in_n_primitives);
        void     draw_primitive_up     (const Interception::Hooks::D3D8::D3DPRIMITIVETYPE& in_type,
                                        const uint32_t&                                    in_n_primitives,
                                        const uint32_t&                                    in_stride);
        /* The next call of a device method returns a negative HRESULT. */
        void                                    end_scene                ();
        void                                    fail_next_device_call    ();
        uint64_t                                get_back_buffer          (const uint32_t&             in_index);
        uint64_t                                get_depth_stencil_surface();
        uint64_t                                get_device_address       () const;
        uint64_t                                get_render_target        ();
        uint64_t                                get_surface_level        (const uint64_t&             in_texture_address,
                                                                          const uint32_t&             in_level);
        const Capture::APIDumpLoader::Workload& get_workload             () const;
        void                                    light_enable             (const uint32_t&             in_index,
                                                                          const bool&                 in_is_enabled);
        /* The data of a lock comes with the unlock. */
        void                                    lock_index_buffer        (const uint64_t&             in_address,
                                                                          const uint32_t&             in_offset,
                                                                          const uint32_t&             in_n_bytes);
        void                                    lock_vertex_buffer       (const uint64_t&             in_address,
                                                                          const uint32_t&             in_offset,
                                                                          const uint32_t&             in_n_bytes);
        APIInterceptor::APIFunctionArgument     make_chunk_argument      (const std::vector<uint8_t>& in_data_u8_vec);
        void                                    present                  ();

        /* The object has the number of references left that is asked for: 0 releases it. */
        void release_surface(const uint64_t& in_address,
                             const uint32_t& in_n_references_left = 0);

        void release_texture        (const uint64_t&                                         in_address,
                                     const uint32_t&                                         in_n_references_left = 0);
        void reset                  ();
        void set_indices            (const uint64_t&                                         in_index_buffer_address,
                                     const uint32_t&                                         in_base_vertex);
        void set_light              (const uint32_t&                                         in_index,
                                     const uint32_t&                                         in_type);
        void set_pixel_shader       (const uint32_t&                                         in_handle);
        void set_render_state       (const Interception::Hooks::D3D8::D3DRENDERSTATETYPE&    in_state,
                                     const uint32_t&                                         in_value);
        void set_render_target      (const uint64_t&                                         in_render_target_address,
                                     const uint64_t&                                         in_depth_stencil_address);
        void set_stream_source      (const uint32_t&                                         in_stream,
                                     const uint64_t&                                         in_vertex_buffer_address,
                                     const uint32_t&                                         in_stride);
        void set_texture            (const uint32_t&                                         in_stage,
                                     const uint64_t&                                         in_texture_address);
        void set_texture_stage_state(const uint32_t&                                         in_stage,
                                     const uint32_t&                                         in_state,
                                     const uint32_t&                                         in_value);
        void set_transform          (const Interception::Hooks::D3D8::D3DTRANSFORMSTATETYPE& in_state,
                                     const Interception::Hooks::D3D8::D3DMATRIX&             in_matrix);
        void set_vertex_shader      (const uint32_t&                                         in_handle);
        void set_viewport           (const Interception::Hooks::D3D8::D3DVIEWPORT8&          in_viewport);
        void unlock_index_buffer    (const uint64_t&                                         in_address,
                                     const std::vector<uint8_t>&                             in_data_u8_vec);
        void unlock_texture_rect    (const uint64_t&                                         in_address,
                                     const uint32_t&                                         in_level,
                                     const std::vector<uint8_t>&                             in_data_u8_vec);
        void unlock_vertex_buffer   (const uint64_t&                                         in_address,
                                     const std::vector<uint8_t>&                             in_data_u8_vec);
        void update_texture         (const uint64_t&                                         in_source_address,
                                     const uint64_t&                                         in_destination_address);

    private:
        /* A Create* call whose last argument is the object written by the call. */
        uint64_t add_creation_call(const APIInterceptor::APIFunction& in_function,
                                   ArgumentVector                     in_arguments,
                                   const uint64_t&                    in_address);

        /* A call to a device method; the device is the first argument. */
        void add_device_call(const APIInterceptor::APIFunction& in_function,
                             const ArgumentVector&              in_arguments);

        static APIInterceptor::APIFunctionArgument make_pointer_argument(const uint64_t& in_address);

        uint64_t                         m_device_address;
        bool                             m_fails_next_device_call;
        uint64_t                         m_next_address;
        uint32_t                         m_next_shader_handle;
        uint64_t                         m_next_timestamp;
        Capture::APIDumpLoader::Workload m_workload;
    };
}

#endif /* TESTS_D3D8_CALL_BUILDER_H */

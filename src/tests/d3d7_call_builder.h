/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Makes the calls of a small DirectDraw 7 and Direct3D 7 application, as the dumper would have recorded them, for the tests of the
 * front end.
 */
#ifndef TESTS_D3D7_CALL_BUILDER_H
#define TESTS_D3D7_CALL_BUILDER_H

#include "Capture/APIDumpLoader/api_dump_loader.h"
#include "Interception/Hooks/D3D7/types.h"
#include <stdint.h>
#include <vector>

namespace Tests
{
    class D3D7CallBuilder
    {
    public:
        typedef std::vector<APIInterceptor::APIFunctionArgument> ArgumentVector;

        /* What CreateSurface is asked for. */
        struct SurfaceSpec
        {
            uint32_t back_buffer_count = 0;
            uint32_t caps              = 0;
            uint32_t flags             = 0;
            uint32_t height            = 0;
            uint32_t mip_map_count     = 0;
            uint32_t pixel_bit_count   = 0;
            uint32_t pixel_flags       = 0;
            uint32_t width             = 0;
        };

        D3D7CallBuilder();

        /* The out arguments default to the in arguments. The returned value is that of a HRESULT-returning method. */
        void add_call(const APIInterceptor::APIFunction&         in_function,
                      const ArgumentVector&                      in_arguments,
                      const ArgumentVector&                      in_out_arguments = {},
                      const APIInterceptor::APIFunctionArgument& in_returned_value = APIInterceptor::APIFunctionArgument::create_i32(0));

        /* The address which the next object will have, as the dumped process would see it. */
        uint64_t allocate_address();

        /* The bytes of an object in memory. */
        static std::vector<uint8_t> get_bytes(const void*   in_data_ptr,
                                              const size_t& in_n_bytes);

        void add_attached_surface(const uint64_t& in_surface_address,
                                  const uint64_t& in_attached_address);
        void begin_state_block   ();
        void blt                 (const uint64_t& in_destination_address,
                                  const uint64_t& in_source_address,
                                  const bool&     in_is_primary);
        void clear               (const uint32_t& in_flags,
                                  const uint32_t& in_color,
                                  const float&    in_depth,
                                  const uint32_t& in_stencil);

        /* The create_* and get_* functions return the address of the object. */
        uint64_t create_device (const uint64_t&    in_surface_address);
        uint64_t create_surface(const SurfaceSpec& in_spec);

        uint64_t create_vertex_buffer(const uint32_t& in_fvf,
                                      const uint32_t& in_n_vertices);

        /* The primary surface of a chain of flipping surfaces, with the number of back buffers. */
        uint64_t create_primary_surface(const uint32_t& in_back_buffer_count);

        /* A surface for a depth buffer, as large as the screen. */
        uint64_t create_depth_buffer();

        /* The vertices of a draw from the application's memory are stored with the call. */
        void     draw_indexed_primitive(const Interception::Hooks::D3D7::D3DPRIMITIVETYPE& in_type,
                                        const uint32_t&                                    in_fvf,
                                        const uint32_t&                                    in_n_vertices,
                                        const std::vector<uint16_t>&                       in_indices_u16_vec,
                                        const std::vector<uint8_t>&                        in_vertices_u8_vec);
        void     draw_primitive       (const Interception::Hooks::D3D7::D3DPRIMITIVETYPE& in_type,
                                       const uint32_t&                                    in_fvf,
                                       const uint32_t&                                    in_n_vertices,
                                       const std::vector<uint8_t>&                        in_vertices_u8_vec = {});
        /* Only the positions are given; the structure of the strides is stored as the dumper does, and the arrays after it. */
        void     draw_primitive_strided(const Interception::Hooks::D3D7::D3DPRIMITIVETYPE& in_type,
                                        const uint32_t&                                    in_fvf,
                                        const uint32_t&                                    in_n_vertices,
                                        const uint32_t&                                    in_position_stride,
                                        const std::vector<uint8_t>&                        in_positions_u8_vec);
        void     draw_primitive_vb    (const Interception::Hooks::D3D7::D3DPRIMITIVETYPE& in_type,
                                       const uint64_t&                                    in_vertex_buffer_address,
                                       const uint32_t&                                    in_start_vertex,
                                       const uint32_t&                                    in_n_vertices);
        void     fail_next_device_call();
        void     flip                 (const uint64_t& in_surface_address);
        uint64_t get_attached_surface (const uint64_t& in_surface_address,
                                       const uint32_t& in_caps,
                                       const uint64_t& in_address = 0);
        uint64_t get_device_address   () const;
        const Capture::APIDumpLoader::Workload& get_workload() const;
        void     release              (const uint64_t& in_address,
                                       const uint32_t& in_n_remaining_references);
        void     set_display_mode     (const uint32_t& in_width,
                                       const uint32_t& in_height);
        void     set_render_state     (const uint32_t& in_state,
                                       const uint32_t& in_value);
        void     set_render_target    (const uint64_t& in_surface_address);
        void     set_texture          (const uint32_t& in_stage,
                                       const uint64_t& in_surface_address);
        void     set_transform        (const uint32_t&                             in_state,
                                       const Interception::Hooks::D3D7::D3DMATRIX& in_matrix);
        void     set_viewport         (const uint32_t& in_x,
                                       const uint32_t& in_y,
                                       const uint32_t& in_width,
                                       const uint32_t& in_height);
        void     unlock               (const uint64_t&             in_surface_address,
                                       const std::vector<uint8_t>& in_data_u8_vec);
        void     unlock_vertex_buffer (const uint64_t&             in_vertex_buffer_address,
                                       const std::vector<uint8_t>& in_data_u8_vec);

    private:
        void add_device_call (const APIInterceptor::APIFunction& in_function,
                              const ArgumentVector&              in_arguments);
        void add_surface_call(const APIInterceptor::APIFunction& in_function,
                              const uint64_t&                    in_surface_address,
                              const ArgumentVector&              in_arguments);

        APIInterceptor::APIFunctionArgument make_chunk_argument  (const std::vector<uint8_t>& in_data_u8_vec);
        APIInterceptor::APIFunctionArgument make_pointer_argument(const uint64_t&              in_address);


        uint64_t                         m_device_address;
        uint64_t                         m_direct3d_address;
        uint64_t                         m_direct_draw_address;
        bool                             m_fails_next_device_call;
        uint64_t                         m_next_address;
        uint64_t                         m_next_timestamp;
        uint32_t                         m_screen_height;
        uint32_t                         m_screen_width;
        Capture::APIDumpLoader::Workload m_workload;
    };
}

#endif /* TESTS_D3D7_CALL_BUILDER_H */

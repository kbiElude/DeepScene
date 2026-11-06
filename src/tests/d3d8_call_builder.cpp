/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d8_call_builder.h"
#include "d3d8_struct_bytes.h"
#include "common/types.h"
#include <string.h>

Tests::D3D8CallBuilder::D3D8CallBuilder()
    :m_device_address     (0),
     m_fails_next_device_call(false),
     m_next_address       (0),
     m_next_shader_handle (0),
     m_next_timestamp     (0)
{
    constexpr uint64_t FIRST_ADDRESS       = 0x10000;
    constexpr uint32_t FIRST_SHADER_HANDLE = 0x20;
    constexpr uint64_t FIRST_TIMESTAMP     = 1000;
    constexpr uint64_t QPC_FREQUENCY       = 10000000;

    m_next_address       = FIRST_ADDRESS;
    m_next_shader_handle = FIRST_SHADER_HANDLE;
    m_next_timestamp     = FIRST_TIMESTAMP;

    m_workload.dumped_api_call_vec_ptr.reset(new Capture::APIDumpLoader::DumpedAPICallVector() );
    m_workload.dumped_surface_vec_ptr.reset (new Capture::APIDumpLoader::DumpedSurfaceVector() );

    m_workload.pointer_n_bytes = sizeof(uint32_t);
    m_workload.qpc_frequency   = QPC_FREQUENCY;
}

void Tests::D3D8CallBuilder::add_call(const APIInterceptor::APIFunction&         in_function,
                                      const ArgumentVector&                      in_arguments,
                                      const ArgumentVector&                      in_out_arguments,
                                      const APIInterceptor::APIFunctionArgument& in_returned_value)
{
    constexpr uint32_t THREAD_ID             = 1;
    constexpr uint64_t TICKS_BETWEEN_CALLS   = 10;

    Capture::APIDumpLoader::DumpedAPICall call;

    call.arg_in_vec         = in_arguments;
    call.arg_out_vec        = (in_out_arguments.empty() ) ? in_arguments
                                                     : in_out_arguments;
    call.func               = in_function;
    call.returned_value_ptr.reset(new APIInterceptor::APIFunctionArgument(in_returned_value) );
    call.thread_id          = THREAD_ID;
    call.timestamp_qpc      = m_next_timestamp;

    m_next_timestamp += TICKS_BETWEEN_CALLS;

    m_workload.dumped_api_call_vec_ptr->emplace_back(std::move(call) );
}

uint64_t Tests::D3D8CallBuilder::add_creation_call(const APIInterceptor::APIFunction& in_function,
                                                   ArgumentVector                     in_arguments,
                                                   const uint64_t&                    in_address)
{
    const uint64_t                                         address  = (in_address != 0) ? in_address
                                                                                        : allocate_address();
    ArgumentVector       out_arguments = in_arguments;

    /* The call writes the new object to where the last argument points. */
    in_arguments.push_back (make_pointer_argument(allocate_address() ));
    out_arguments.push_back(make_pointer_argument(address) );

    add_call(in_function,
             in_arguments,
             out_arguments);

    return address;
}

void Tests::D3D8CallBuilder::add_device_call(const APIInterceptor::APIFunction& in_function,
                                             const ArgumentVector&              in_arguments)
{
    ArgumentVector arguments;

    arguments.push_back(make_pointer_argument(m_device_address) );
    arguments.insert   (arguments.end(),
                   in_arguments.begin(),
                   in_arguments.end() );

    add_call(in_function,
             arguments,
             {},
             APIInterceptor::APIFunctionArgument::create_i32((m_fails_next_device_call) ? -1
                                                                                         : 0) );

    m_fails_next_device_call = false;
}

uint64_t Tests::D3D8CallBuilder::allocate_address()
{
    constexpr uint64_t ADDRESS_STEP = 0x10;

    const uint64_t result = m_next_address;

    m_next_address += ADDRESS_STEP;

    return result;
}

void Tests::D3D8CallBuilder::begin_scene()
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_BEGINSCENE,
                    {});
}

void Tests::D3D8CallBuilder::begin_state_block()
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_BEGINSTATEBLOCK,
                    {});
}

void Tests::D3D8CallBuilder::clear(const uint32_t& in_flags,
                                   const uint32_t& in_color,
                                   const float&    in_depth,
                                   const uint32_t& in_stencil)
{
    constexpr uint32_t N_RECTS = 0;

    /* The rectangles, if any, follow the arguments of the method, in a data chunk. */
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR,
                    {APIInterceptor::APIFunctionArgument::create_u32     (N_RECTS),
                     APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                     APIInterceptor::APIFunctionArgument::create_u32     (in_flags),
                     APIInterceptor::APIFunctionArgument::create_u32     (in_color),
                     APIInterceptor::APIFunctionArgument::create_fp32    (in_depth),
                     APIInterceptor::APIFunctionArgument::create_u32     (in_stencil),
                     make_chunk_argument                                 ({})});
}

void Tests::D3D8CallBuilder::copy_rects(const uint64_t& in_source_address,
                                        const uint64_t& in_destination_address)
{
    constexpr uint32_t N_RECTS = 0;

    /* The rectangles and the points follow the arguments of the method, in data chunks. */
    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_COPYRECTS,
             {make_pointer_argument                             (m_device_address),
              make_pointer_argument                             (in_source_address),
              APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
              APIInterceptor::APIFunctionArgument::create_u32     (N_RECTS),
              make_pointer_argument                             (in_destination_address),
              APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
              make_chunk_argument                               ({}),
              make_chunk_argument                               ({})});
}

uint64_t Tests::D3D8CallBuilder::create_depth_stencil_surface(const uint32_t&                             in_width,
                                                              const uint32_t&                             in_height,
                                                              const Interception::Hooks::D3D8::D3DFORMAT& in_format,
                                                              const uint64_t&                             in_address)
{
    ArgumentVector arguments;

    arguments.push_back(make_pointer_argument(m_device_address) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_width) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_height) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_format) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (Interception::Hooks::D3D8::D3DMULTISAMPLE_NONE) );

    return add_creation_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEDEPTHSTENCILSURFACE,
                             arguments,
                             in_address);
}

uint64_t Tests::D3D8CallBuilder::create_device()
{
    constexpr uint32_t ADAPTER              = 0;
    constexpr uint32_t BEHAVIOR_FLAGS       = 64;  /* D3DCREATE_HARDWARE_VERTEXPROCESSING */
    constexpr uint32_t DIRECT3D_SDK_VERSION = 120;

    const uint64_t d3d8_address = allocate_address();

    ArgumentVector arguments;
    ArgumentVector out_arguments;

    add_call(APIInterceptor::APIFUNCTION_D3D8_DIRECT3DCREATE8,
             {APIInterceptor::APIFunctionArgument::create_u32(DIRECT3D_SDK_VERSION)},
             {},
             make_pointer_argument(d3d8_address) );

    arguments.push_back(make_pointer_argument(d3d8_address) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (ADAPTER) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (Interception::Hooks::D3D8::D3DDEVTYPE_HAL) );
    arguments.push_back(make_pointer_argument(allocate_address() ));
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (BEHAVIOR_FLAGS) );
    arguments.push_back(make_chunk_argument(D3D8StructBytes::build_present_parameters(m_workload.pointer_n_bytes) ));

    m_device_address = add_creation_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3D8_CREATEDEVICE,
                                         arguments,
                                         0);

    return m_device_address;
}

uint64_t Tests::D3D8CallBuilder::create_index_buffer(const uint32_t& in_length,
                                                     const uint64_t& in_address)
{
    constexpr uint32_t FORMAT_INDEX16 = 101;
    constexpr uint32_t POOL_DEFAULT   = 0;
    constexpr uint32_t USAGE_DYNAMIC  = 512;

    ArgumentVector arguments;

    arguments.push_back(make_pointer_argument(m_device_address) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_length) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (USAGE_DYNAMIC) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (FORMAT_INDEX16) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (POOL_DEFAULT) );

    return add_creation_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEINDEXBUFFER,
                             arguments,
                             in_address);
}

uint32_t Tests::D3D8CallBuilder::create_pixel_shader(const std::vector<uint8_t>& in_function_u8_vec)
{
    constexpr uint32_t HANDLE_ARGUMENT = 2;

    const uint32_t handle = m_next_shader_handle++;

    ArgumentVector arguments = {make_pointer_argument                             (m_device_address),
                                APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                                make_pointer_argument                             (allocate_address() ),
                                make_chunk_argument                               (in_function_u8_vec)};
    ArgumentVector out_arguments = arguments;

    /* The handle is written through a pointer; after the call, the argument holds it. */
    out_arguments.at(HANDLE_ARGUMENT) = APIInterceptor::APIFunctionArgument::create_u32(handle);

    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEPIXELSHADER,
             arguments,
             out_arguments);

    return handle;
}

uint64_t Tests::D3D8CallBuilder::create_texture(const uint32_t&                             in_width,
                                                const uint32_t&                             in_height,
                                                const uint32_t&                             in_n_levels,
                                                const uint32_t&                             in_usage,
                                                const Interception::Hooks::D3D8::D3DFORMAT& in_format,
                                                const uint64_t&                             in_address)
{
    ArgumentVector arguments;

    arguments.push_back(make_pointer_argument(m_device_address) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_width) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_height) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_n_levels) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_usage) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_format) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (Interception::Hooks::D3D8::D3DPOOL_DEFAULT) );

    return add_creation_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATETEXTURE,
                             arguments,
                             in_address);
}

uint64_t Tests::D3D8CallBuilder::create_vertex_buffer(const uint32_t& in_length,
                                                      const uint32_t& in_fvf,
                                                      const uint64_t& in_address)
{
    constexpr uint32_t POOL_DEFAULT  = 0;
    constexpr uint32_t USAGE_DYNAMIC = 512;

    ArgumentVector arguments;

    arguments.push_back(make_pointer_argument(m_device_address) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_length) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (USAGE_DYNAMIC) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_fvf) );
    arguments.push_back(APIInterceptor::APIFunctionArgument::create_u32     (POOL_DEFAULT) );

    return add_creation_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVERTEXBUFFER,
                             arguments,
                             in_address);
}

uint32_t Tests::D3D8CallBuilder::create_vertex_shader(const std::vector<uint8_t>& in_declaration_u8_vec,
                                                      const std::vector<uint8_t>& in_function_u8_vec)
{
    constexpr uint32_t HANDLE_ARGUMENT = 3;
    constexpr uint32_t USAGE           = 0;

    const uint32_t handle = m_next_shader_handle++;

    ArgumentVector arguments = {make_pointer_argument                             (m_device_address),
                                APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                                APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                                make_pointer_argument                             (allocate_address() ),
                                APIInterceptor::APIFunctionArgument::create_u32     (USAGE),
                                make_chunk_argument                               (in_declaration_u8_vec),
                                make_chunk_argument                               (in_function_u8_vec)};
    ArgumentVector out_arguments = arguments;

    out_arguments.at(HANDLE_ARGUMENT) = APIInterceptor::APIFunctionArgument::create_u32(handle);

    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVERTEXSHADER,
             arguments,
             out_arguments);

    return handle;
}

void Tests::D3D8CallBuilder::delete_pixel_shader(const uint32_t& in_handle)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DELETEPIXELSHADER,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_handle)});
}

void Tests::D3D8CallBuilder::delete_vertex_shader(const uint32_t& in_handle)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DELETEVERTEXSHADER,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_handle)});
}

void Tests::D3D8CallBuilder::draw_indexed_primitive(const Interception::Hooks::D3D8::D3DPRIMITIVETYPE& in_type,
                                                    const uint32_t&                                    in_min_index,
                                                    const uint32_t&                                    in_n_vertices,
                                                    const uint32_t&                                    in_start_index,
                                                    const uint32_t&                                    in_n_primitives)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_type),
                     APIInterceptor::APIFunctionArgument::create_u32(in_min_index),
                     APIInterceptor::APIFunctionArgument::create_u32(in_n_vertices),
                     APIInterceptor::APIFunctionArgument::create_u32(in_start_index),
                     APIInterceptor::APIFunctionArgument::create_u32(in_n_primitives)});
}

void Tests::D3D8CallBuilder::draw_primitive(const Interception::Hooks::D3D8::D3DPRIMITIVETYPE& in_type,
                                            const uint32_t&                                    in_start_vertex,
                                            const uint32_t&                                    in_n_primitives)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_type),
                     APIInterceptor::APIFunctionArgument::create_u32(in_start_vertex),
                     APIInterceptor::APIFunctionArgument::create_u32(in_n_primitives)});
}

void Tests::D3D8CallBuilder::draw_primitive_up(const Interception::Hooks::D3D8::D3DPRIMITIVETYPE& in_type,
                                               const uint32_t&                                    in_n_primitives,
                                               const uint32_t&                                    in_stride)
{
    /* The vertices follow the arguments of the method, in a data chunk. */
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP,
                    {APIInterceptor::APIFunctionArgument::create_u32     (in_type),
                     APIInterceptor::APIFunctionArgument::create_u32     (in_n_primitives),
                     APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                     APIInterceptor::APIFunctionArgument::create_u32     (in_stride),
                     make_chunk_argument                                 ({})});
}

void Tests::D3D8CallBuilder::end_scene()
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_ENDSCENE,
                    {});
}

void Tests::D3D8CallBuilder::fail_next_device_call()
{
    m_fails_next_device_call = true;
}

uint64_t Tests::D3D8CallBuilder::get_back_buffer(const uint32_t& in_index)
{
    constexpr uint32_t BACK_BUFFER_TYPE_MONO = 0;

    const uint64_t result = allocate_address();

    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETBACKBUFFER,
             {make_pointer_argument(m_device_address),
              APIInterceptor::APIFunctionArgument::create_u32(in_index),
              APIInterceptor::APIFunctionArgument::create_u32(BACK_BUFFER_TYPE_MONO),
              make_pointer_argument(allocate_address() )},
             {make_pointer_argument(m_device_address),
              APIInterceptor::APIFunctionArgument::create_u32(in_index),
              APIInterceptor::APIFunctionArgument::create_u32(BACK_BUFFER_TYPE_MONO),
              make_pointer_argument(result)});

    return result;
}

uint64_t Tests::D3D8CallBuilder::get_depth_stencil_surface()
{
    const uint64_t result = allocate_address();

    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETDEPTHSTENCILSURFACE,
             {make_pointer_argument(m_device_address),
              make_pointer_argument(allocate_address() )},
             {make_pointer_argument(m_device_address),
              make_pointer_argument(result) });

    return result;
}

uint64_t Tests::D3D8CallBuilder::get_device_address() const
{
    return m_device_address;
}

uint64_t Tests::D3D8CallBuilder::get_render_target()
{
    const uint64_t result = allocate_address();

    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETRENDERTARGET,
             {make_pointer_argument(m_device_address),
              make_pointer_argument(allocate_address() )},
             {make_pointer_argument(m_device_address),
              make_pointer_argument(result) });

    return result;
}

uint64_t Tests::D3D8CallBuilder::get_surface_level(const uint64_t& in_texture_address,
                                                   const uint32_t& in_level)
{
    const uint64_t result = allocate_address();

    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_GETSURFACELEVEL,
             {make_pointer_argument(in_texture_address),
              APIInterceptor::APIFunctionArgument::create_u32     (in_level),
              make_pointer_argument(allocate_address() )},
             {make_pointer_argument(in_texture_address),
              APIInterceptor::APIFunctionArgument::create_u32     (in_level),
              make_pointer_argument(result) });

    return result;
}

const Capture::APIDumpLoader::Workload& Tests::D3D8CallBuilder::get_workload() const
{
    return m_workload;
}

void Tests::D3D8CallBuilder::light_enable(const uint32_t& in_index,
                                          const bool&     in_is_enabled)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_LIGHTENABLE,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_index),
                     APIInterceptor::APIFunctionArgument::create_i32((in_is_enabled) ? 1 : 0)});
}

void Tests::D3D8CallBuilder::lock_index_buffer(const uint64_t& in_address,
                                               const uint32_t& in_offset,
                                               const uint32_t& in_n_bytes)
{
    constexpr uint32_t NO_FLAGS = 0;

    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_LOCK,
             {make_pointer_argument                              (in_address),
              APIInterceptor::APIFunctionArgument::create_u32    (in_offset),
              APIInterceptor::APIFunctionArgument::create_u32    (in_n_bytes),
              make_pointer_argument                              (0),
              APIInterceptor::APIFunctionArgument::create_u32    (NO_FLAGS)});
}

void Tests::D3D8CallBuilder::lock_vertex_buffer(const uint64_t& in_address,
                                                const uint32_t& in_offset,
                                                const uint32_t& in_n_bytes)
{
    constexpr uint32_t NO_FLAGS = 0;

    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_LOCK,
             {make_pointer_argument                              (in_address),
              APIInterceptor::APIFunctionArgument::create_u32    (in_offset),
              APIInterceptor::APIFunctionArgument::create_u32    (in_n_bytes),
              make_pointer_argument                              (0),
              APIInterceptor::APIFunctionArgument::create_u32    (NO_FLAGS)});
}

APIInterceptor::APIFunctionArgument Tests::D3D8CallBuilder::make_chunk_argument(const std::vector<uint8_t>& in_data_u8_vec)
{
    const uint32_t chunk_id = static_cast<uint32_t>(m_workload.data_chunk_ptr_vec.size() );

    m_workload.data_chunk_ptr_vec.emplace_back(new Capture::APIDumpLoader::DataChunk(in_data_u8_vec) );

    return APIInterceptor::APIFunctionArgument::create_data_chunk_id(chunk_id);
}

APIInterceptor::APIFunctionArgument Tests::D3D8CallBuilder::make_pointer_argument(const uint64_t& in_address)
{
    return APIInterceptor::APIFunctionArgument::create_void_ptr(reinterpret_cast<void*>(static_cast<uintptr_t>(in_address) ));
}

void Tests::D3D8CallBuilder::present()
{
    /* The rectangles and the region follow the arguments of the method, in data chunks. */
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT,
                    {APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                     APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                     APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                     APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                     make_chunk_argument({}),
                     make_chunk_argument({}),
                     make_chunk_argument({})});
}

void Tests::D3D8CallBuilder::release_surface(const uint64_t& in_address,
                                             const uint32_t& in_n_references_left)
{
    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_RELEASE,
             {make_pointer_argument(in_address) },
             {},
             APIInterceptor::APIFunctionArgument::create_u32(in_n_references_left) );
}

void Tests::D3D8CallBuilder::release_texture(const uint64_t& in_address,
                                             const uint32_t& in_n_references_left)
{
    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_RELEASE,
             {make_pointer_argument(in_address) },
             {},
             APIInterceptor::APIFunctionArgument::create_u32(in_n_references_left) );
}

void Tests::D3D8CallBuilder::reset()
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_RESET,
                    {make_chunk_argument(D3D8StructBytes::build_present_parameters(m_workload.pointer_n_bytes) )});
}

void Tests::D3D8CallBuilder::set_indices(const uint64_t& in_index_buffer_address,
                                         const uint32_t& in_base_vertex)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETINDICES,
                    {make_pointer_argument(in_index_buffer_address),
                     APIInterceptor::APIFunctionArgument::create_u32     (in_base_vertex)});
}

void Tests::D3D8CallBuilder::set_light(const uint32_t& in_index,
                                       const uint32_t& in_type)
{
    constexpr size_t LIGHT_N_BYTES = 104; /* sizeof(D3DLIGHT8) */

    std::vector<uint8_t> light_u8_vec(LIGHT_N_BYTES,
                                      0);

    /* The type is the first field. */
    ::memcpy(light_u8_vec.data(),
            &in_type,
             sizeof(in_type) );

    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETLIGHT,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_index),
                     make_chunk_argument                            (light_u8_vec)});
}

void Tests::D3D8CallBuilder::set_pixel_shader(const uint32_t& in_handle)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETPIXELSHADER,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_handle)});
}

void Tests::D3D8CallBuilder::set_render_state(const Interception::Hooks::D3D8::D3DRENDERSTATETYPE& in_state,
                                              const uint32_t&                                      in_value)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERSTATE,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_state),
                     APIInterceptor::APIFunctionArgument::create_u32(in_value)});
}

void Tests::D3D8CallBuilder::set_render_target(const uint64_t& in_render_target_address,
                                               const uint64_t& in_depth_stencil_address)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERTARGET,
                    {make_pointer_argument(in_render_target_address),
                     make_pointer_argument(in_depth_stencil_address) });
}

void Tests::D3D8CallBuilder::set_stream_source(const uint32_t& in_stream,
                                               const uint64_t& in_vertex_buffer_address,
                                               const uint32_t& in_stride)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETSTREAMSOURCE,
                    {APIInterceptor::APIFunctionArgument::create_u32     (in_stream),
                     make_pointer_argument(in_vertex_buffer_address),
                     APIInterceptor::APIFunctionArgument::create_u32     (in_stride)});
}

void Tests::D3D8CallBuilder::set_texture(const uint32_t& in_stage,
                                         const uint64_t& in_texture_address)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTEXTURE,
                    {APIInterceptor::APIFunctionArgument::create_u32     (in_stage),
                     make_pointer_argument(in_texture_address) });
}

void Tests::D3D8CallBuilder::set_texture_stage_state(const uint32_t& in_stage,
                                                     const uint32_t& in_state,
                                                     const uint32_t& in_value)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTEXTURESTAGESTATE,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_stage),
                     APIInterceptor::APIFunctionArgument::create_u32(in_state),
                     APIInterceptor::APIFunctionArgument::create_u32(in_value)});
}

void Tests::D3D8CallBuilder::set_transform(const Interception::Hooks::D3D8::D3DTRANSFORMSTATETYPE& in_state,
                                           const Interception::Hooks::D3D8::D3DMATRIX&             in_matrix)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_state),
                     make_chunk_argument                            (D3D8StructBytes::build_matrix(in_matrix) )});
}

void Tests::D3D8CallBuilder::set_vertex_shader(const uint32_t& in_handle)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADER,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_handle)});
}

void Tests::D3D8CallBuilder::set_viewport(const Interception::Hooks::D3D8::D3DVIEWPORT8& in_viewport)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVIEWPORT,
                    {make_chunk_argument(D3D8StructBytes::build_viewport(in_viewport) )});
}

void Tests::D3D8CallBuilder::unlock_index_buffer(const uint64_t&             in_address,
                                                 const std::vector<uint8_t>& in_data_u8_vec)
{
    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_UNLOCK,
             {make_pointer_argument(in_address),
              make_chunk_argument                                 (in_data_u8_vec)});
}

void Tests::D3D8CallBuilder::unlock_texture_rect(const uint64_t&             in_address,
                                                 const uint32_t&             in_level,
                                                 const std::vector<uint8_t>& in_data_u8_vec)
{
    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_UNLOCKRECT,
             {make_pointer_argument                         (in_address),
              APIInterceptor::APIFunctionArgument::create_u32(in_level),
              make_chunk_argument                           (in_data_u8_vec)});
}

void Tests::D3D8CallBuilder::unlock_vertex_buffer(const uint64_t&             in_address,
                                                  const std::vector<uint8_t>& in_data_u8_vec)
{
    add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_UNLOCK,
             {make_pointer_argument(in_address),
              make_chunk_argument                                 (in_data_u8_vec)});
}

void Tests::D3D8CallBuilder::update_texture(const uint64_t& in_source_address,
                                            const uint64_t& in_destination_address)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_UPDATETEXTURE,
                    {make_pointer_argument(in_source_address),
                     make_pointer_argument(in_destination_address)});
}

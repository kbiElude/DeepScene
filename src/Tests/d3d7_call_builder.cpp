/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d7_call_builder.h"
#include "Common/types.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include <string.h>

Tests::D3D7CallBuilder::D3D7CallBuilder()
    :m_device_address        (0),
     m_direct_draw_address   (0),
     m_fails_next_device_call(false),
     m_next_address          (0),
     m_next_timestamp        (0),
     m_screen_height         (0),
     m_screen_width          (0)
{
    constexpr uint64_t FIRST_ADDRESS   = 0x10000;
    constexpr uint64_t FIRST_TIMESTAMP = 1000;
    constexpr uint64_t QPC_FREQUENCY   = 10000000;

    m_next_address   = FIRST_ADDRESS;
    m_next_timestamp = FIRST_TIMESTAMP;

    m_workload.dumped_api_call_vec_ptr.reset(new Capture::APIDumpLoader::DumpedAPICallVector() );
    m_workload.dumped_surface_vec_ptr.reset (new Capture::APIDumpLoader::DumpedSurfaceVector() );

    m_workload.pointer_n_bytes = sizeof(uint32_t);
    m_workload.qpc_frequency   = QPC_FREQUENCY;

    m_direct_draw_address = allocate_address();
}

void Tests::D3D7CallBuilder::add_attached_surface(const uint64_t& in_surface_address,
                                                  const uint64_t& in_attached_address)
{
    add_surface_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_ADDATTACHEDSURFACE,
                     in_surface_address,
                     {make_pointer_argument(in_attached_address)});
}

void Tests::D3D7CallBuilder::add_call(const APIInterceptor::APIFunction&         in_function,
                                      const ArgumentVector&                      in_arguments,
                                      const ArgumentVector&                      in_out_arguments,
                                      const APIInterceptor::APIFunctionArgument& in_returned_value)
{
    constexpr uint32_t THREAD_ID           = 1;
    constexpr uint64_t TICKS_BETWEEN_CALLS = 10;

    Capture::APIDumpLoader::DumpedAPICall call;

    call.arg_in_vec  = in_arguments;
    call.arg_out_vec = (in_out_arguments.empty() ) ? in_arguments
                                                   : in_out_arguments;
    call.func        = in_function;
    call.returned_value_ptr.reset(new APIInterceptor::APIFunctionArgument(in_returned_value) );
    call.thread_id     = THREAD_ID;
    call.timestamp_qpc = m_next_timestamp;

    m_next_timestamp += TICKS_BETWEEN_CALLS;

    m_workload.dumped_api_call_vec_ptr->emplace_back(std::move(call) );
}

void Tests::D3D7CallBuilder::add_device_call(const APIInterceptor::APIFunction& in_function,
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

void Tests::D3D7CallBuilder::add_surface_call(const APIInterceptor::APIFunction& in_function,
                                              const uint64_t&                    in_surface_address,
                                              const ArgumentVector&              in_arguments)
{
    ArgumentVector arguments;

    arguments.push_back(make_pointer_argument(in_surface_address) );
    arguments.insert   (arguments.end(),
                        in_arguments.begin(),
                        in_arguments.end() );

    add_call(in_function,
             arguments);
}

uint64_t Tests::D3D7CallBuilder::allocate_address()
{
    constexpr uint64_t ADDRESS_STEP = 0x10;

    const uint64_t result = m_next_address;

    m_next_address += ADDRESS_STEP;

    return result;
}

void Tests::D3D7CallBuilder::begin_state_block()
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_BEGINSTATEBLOCK,
                    {});
}

void Tests::D3D7CallBuilder::blt(const uint64_t& in_destination_address,
                                 const uint64_t& in_source_address,
                                 const bool&     in_is_primary)
{
    constexpr uint32_t FLAGS = 0;

    /* The rectangles and the effects follow as data, and a flag added by the dumper tells if the destination is the primary surface. */
    add_surface_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT,
                     in_destination_address,
                     {make_chunk_argument({}),
                      make_pointer_argument(in_source_address),
                      make_chunk_argument({}),
                      APIInterceptor::APIFunctionArgument::create_u32(FLAGS),
                      make_chunk_argument({}),
                      APIInterceptor::APIFunctionArgument::create_u32((in_is_primary) ? 1 : 0)});
}

void Tests::D3D7CallBuilder::clear(const uint32_t& in_flags,
                                   const uint32_t& in_color,
                                   const float&    in_depth,
                                   const uint32_t& in_stencil)
{
    constexpr uint32_t N_RECTS = 0;

    /* The rectangles, if any, follow the arguments of the method, in a data chunk. */
    add_device_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR,
                    {APIInterceptor::APIFunctionArgument::create_u32     (N_RECTS),
                     make_chunk_argument                                 ({}),
                     APIInterceptor::APIFunctionArgument::create_u32     (in_flags),
                     APIInterceptor::APIFunctionArgument::create_u32     (in_color),
                     APIInterceptor::APIFunctionArgument::create_fp32    (in_depth),
                     APIInterceptor::APIFunctionArgument::create_u32     (in_stencil),
                     make_chunk_argument                                 ({})});
}

uint64_t Tests::D3D7CallBuilder::create_depth_buffer()
{
    constexpr uint32_t DEPTH_BIT_COUNT = 16;

    SurfaceSpec spec;

    spec.caps            = Interception::Core::D3D7Utils::DDSCAPS_VIDEOMEMORY | Interception::Core::D3D7Utils::DDSCAPS_ZBUFFER;
    spec.flags           = Interception::Core::D3D7Utils::DDSD_CAPS | Interception::Core::D3D7Utils::DDSD_HEIGHT | Interception::Core::D3D7Utils::DDSD_PIXELFORMAT | Interception::Core::D3D7Utils::DDSD_WIDTH;
    spec.height          = m_screen_height;
    spec.pixel_bit_count = DEPTH_BIT_COUNT;
    spec.pixel_flags     = Interception::Core::D3D7Utils::DDPF_ZBUFFER;
    spec.width           = m_screen_width;

    return create_surface(spec);
}

uint64_t Tests::D3D7CallBuilder::create_device(const uint64_t& in_surface_address)
{
    constexpr size_t N_CLSID_BYTES = 16;

    const uint64_t       d3d_address = allocate_address();
    std::vector<uint8_t> clsid_u8_vec(N_CLSID_BYTES,
                                      0);

    m_device_address = allocate_address();

    /* The device renders to the surface that it is made with; it is written to where the last argument points. */
    add_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3D7_CREATEDEVICE,
             {make_pointer_argument(d3d_address),
              make_chunk_argument  (clsid_u8_vec),
              make_pointer_argument(in_surface_address),
              make_pointer_argument(allocate_address() )},
             {make_pointer_argument(d3d_address),
              make_chunk_argument  (clsid_u8_vec),
              make_pointer_argument(in_surface_address),
              make_pointer_argument(m_device_address)});

    return m_device_address;
}

uint64_t Tests::D3D7CallBuilder::create_primary_surface(const uint32_t& in_back_buffer_count)
{
    SurfaceSpec spec;

    spec.back_buffer_count = in_back_buffer_count;
    spec.caps              = Interception::Core::D3D7Utils::DDSCAPS_COMPLEX | Interception::Core::D3D7Utils::DDSCAPS_FLIP | Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE | Interception::Core::D3D7Utils::DDSCAPS_3DDEVICE;
    spec.flags             = Interception::Core::D3D7Utils::DDSD_BACKBUFFERCOUNT | Interception::Core::D3D7Utils::DDSD_CAPS;

    return create_surface(spec);
}

uint64_t Tests::D3D7CallBuilder::create_surface(const SurfaceSpec& in_spec)
{
    Interception::Hooks::D3D7::DDSURFACEDESC2 description = {};
    std::vector<uint8_t>                      description_u8_vec(sizeof(description) );

    description.dwBackBufferCount          = in_spec.back_buffer_count;
    description.dwFlags                    = in_spec.flags;
    description.dwHeight                   = in_spec.height;
    description.dwMipMapCount              = in_spec.mip_map_count;
    description.dwSize                     = static_cast<DWORD>(sizeof(description) );
    description.dwWidth                    = in_spec.width;
    description.ddpfPixelFormat.dwFlags    = in_spec.pixel_flags;
    description.ddpfPixelFormat.dwRGBBitCount = in_spec.pixel_bit_count;
    description.ddsCaps.dwCaps             = in_spec.caps;

    memcpy(description_u8_vec.data(),
           &description,
           sizeof(description) );

    const uint64_t address = allocate_address();

    /* The surface is written to where the third argument points. */
    add_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATESURFACE,
             {make_pointer_argument(m_direct_draw_address),
              make_chunk_argument  (description_u8_vec),
              make_pointer_argument(allocate_address() ),
              make_pointer_argument(0)},
             {make_pointer_argument(m_direct_draw_address),
              make_chunk_argument  (description_u8_vec),
              make_pointer_argument(address),
              make_pointer_argument(0)});

    return address;
}

void Tests::D3D7CallBuilder::draw_primitive(const Interception::Hooks::D3D7::D3DPRIMITIVETYPE& in_type,
                                            const uint32_t&                                    in_fvf,
                                            const uint32_t&                                    in_n_vertices)
{
    constexpr uint32_t FLAGS = 0;

    /* The vertices follow the arguments of the method, in a data chunk. */
    add_device_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_type),
                     APIInterceptor::APIFunctionArgument::create_u32(in_fvf),
                     make_pointer_argument                          (allocate_address() ),
                     APIInterceptor::APIFunctionArgument::create_u32(in_n_vertices),
                     APIInterceptor::APIFunctionArgument::create_u32(FLAGS),
                     make_chunk_argument                            ({})});
}

void Tests::D3D7CallBuilder::fail_next_device_call()
{
    m_fails_next_device_call = true;
}

void Tests::D3D7CallBuilder::flip(const uint64_t& in_surface_address)
{
    constexpr uint32_t FLAGS = 1; /* DDFLIP_WAIT */

    add_surface_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP,
                     in_surface_address,
                     {make_pointer_argument                          (0),
                      APIInterceptor::APIFunctionArgument::create_u32(FLAGS)});
}

uint64_t Tests::D3D7CallBuilder::get_attached_surface(const uint64_t& in_surface_address,
                                                      const uint32_t& in_caps,
                                                      const uint64_t& in_address)
{
    constexpr size_t N_CAPS_BYTES = sizeof(Interception::Hooks::D3D7::DDSCAPS2);

    const uint64_t       address = (in_address != 0) ? in_address
                                                     : allocate_address();
    std::vector<uint8_t> caps_u8_vec(N_CAPS_BYTES);

    memcpy(caps_u8_vec.data(),
           &in_caps,
           sizeof(in_caps) );

    add_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE,
             {make_pointer_argument(in_surface_address),
              make_chunk_argument  (caps_u8_vec),
              make_pointer_argument(allocate_address() )},
             {make_pointer_argument(in_surface_address),
              make_chunk_argument  (caps_u8_vec),
              make_pointer_argument(address)});

    return address;
}

uint64_t Tests::D3D7CallBuilder::get_device_address() const
{
    return m_device_address;
}

const Capture::APIDumpLoader::Workload& Tests::D3D7CallBuilder::get_workload() const
{
    return m_workload;
}

APIInterceptor::APIFunctionArgument Tests::D3D7CallBuilder::make_chunk_argument(const std::vector<uint8_t>& in_data_u8_vec)
{
    const uint32_t chunk_id = static_cast<uint32_t>(m_workload.data_chunk_ptr_vec.size() );

    m_workload.data_chunk_ptr_vec.emplace_back(new Capture::APIDumpLoader::DataChunk(in_data_u8_vec) );

    return APIInterceptor::APIFunctionArgument::create_data_chunk_id(chunk_id);
}

APIInterceptor::APIFunctionArgument Tests::D3D7CallBuilder::make_pointer_argument(const uint64_t& in_address)
{
    return APIInterceptor::APIFunctionArgument::create_void_ptr(reinterpret_cast<void*>(static_cast<uintptr_t>(in_address) ));
}

void Tests::D3D7CallBuilder::release(const uint64_t& in_address,
                                     const uint32_t& in_n_remaining_references)
{
    add_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_RELEASE,
             {make_pointer_argument(in_address)},
             {},
             APIInterceptor::APIFunctionArgument::create_u32(in_n_remaining_references) );
}

void Tests::D3D7CallBuilder::set_display_mode(const uint32_t& in_width,
                                              const uint32_t& in_height)
{
    constexpr uint32_t BITS_PER_PIXEL = 32;
    constexpr uint32_t FLAGS          = 0;
    constexpr uint32_t REFRESH_RATE   = 0;

    m_screen_height = in_height;
    m_screen_width  = in_width;

    add_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_SETDISPLAYMODE,
             {make_pointer_argument                          (m_direct_draw_address),
              APIInterceptor::APIFunctionArgument::create_u32(in_width),
              APIInterceptor::APIFunctionArgument::create_u32(in_height),
              APIInterceptor::APIFunctionArgument::create_u32(BITS_PER_PIXEL),
              APIInterceptor::APIFunctionArgument::create_u32(REFRESH_RATE),
              APIInterceptor::APIFunctionArgument::create_u32(FLAGS)});
}

void Tests::D3D7CallBuilder::set_render_state(const uint32_t& in_state,
                                              const uint32_t& in_value)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_state),
                     APIInterceptor::APIFunctionArgument::create_u32(in_value)});
}

void Tests::D3D7CallBuilder::set_render_target(const uint64_t& in_surface_address)
{
    constexpr uint32_t FLAGS = 0;

    add_device_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERTARGET,
                    {make_pointer_argument                          (in_surface_address),
                     APIInterceptor::APIFunctionArgument::create_u32(FLAGS)});
}

void Tests::D3D7CallBuilder::set_texture(const uint32_t& in_stage,
                                         const uint64_t& in_surface_address)
{
    add_device_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTEXTURE,
                    {APIInterceptor::APIFunctionArgument::create_u32(in_stage),
                     make_pointer_argument                          (in_surface_address)});
}

void Tests::D3D7CallBuilder::set_viewport(const uint32_t& in_x,
                                          const uint32_t& in_y,
                                          const uint32_t& in_width,
                                          const uint32_t& in_height)
{
    Interception::Hooks::D3D7::D3DVIEWPORT7 viewport = {};
    std::vector<uint8_t>                    viewport_u8_vec(sizeof(viewport) );

    viewport.dvMaxZ   = 1.0f;
    viewport.dwHeight = in_height;
    viewport.dwWidth  = in_width;
    viewport.dwX      = in_x;
    viewport.dwY      = in_y;

    memcpy(viewport_u8_vec.data(),
           &viewport,
           sizeof(viewport) );

    add_device_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETVIEWPORT,
                    {make_chunk_argument(viewport_u8_vec)});
}

void Tests::D3D7CallBuilder::unlock(const uint64_t&             in_surface_address,
                                    const std::vector<uint8_t>& in_data_u8_vec)
{
    /* What the application has written follows the arguments of the method. */
    add_surface_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_UNLOCK,
                     in_surface_address,
                     {make_chunk_argument({}),
                      make_chunk_argument(in_data_u8_vec)});
}

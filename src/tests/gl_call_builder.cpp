/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_call_builder.h"
#include "common/types.h"
#include <memory>
#include <string.h>

Tests::GlCallBuilder::GlCallBuilder()
    :m_next_timestamp(0),
     m_thread_id     (0)
{
    constexpr uint32_t FIRST_THREAD_ID = 1;
    constexpr uint64_t FIRST_TIMESTAMP = 1000;
    constexpr uint64_t QPC_FREQUENCY   = 10000000;

    m_next_timestamp = FIRST_TIMESTAMP;
    m_thread_id      = FIRST_THREAD_ID;

    m_workload.dumped_api_call_vec_ptr.reset(new Capture::APIDumpLoader::DumpedAPICallVector() );
    m_workload.dumped_surface_vec_ptr.reset (new Capture::APIDumpLoader::DumpedSurfaceVector() );

    m_workload.pointer_n_bytes = sizeof(uint32_t);
    m_workload.qpc_frequency   = QPC_FREQUENCY;
}

void Tests::GlCallBuilder::active_texture(const uint32_t& in_unit)
{
    constexpr uint32_t TEXTURE0 = 0x84C0;

    add_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(TEXTURE0 + in_unit)});
}

void Tests::GlCallBuilder::add_call(const APIInterceptor::APIFunction&         in_function,
                                    const ArgumentVector&                      in_arguments,
                                    const APIInterceptor::APIFunctionArgument& in_returned_value)
{
    constexpr uint64_t TICKS_BETWEEN_CALLS = 10;

    Capture::APIDumpLoader::DumpedAPICall call;

    call.arg_in_vec  = in_arguments;
    call.arg_out_vec = in_arguments;
    call.func        = in_function;

    call.returned_value_ptr.reset(new APIInterceptor::APIFunctionArgument(in_returned_value) );

    call.thread_id     = m_thread_id;
    call.timestamp_qpc = m_next_timestamp;

    m_next_timestamp += TICKS_BETWEEN_CALLS;

    m_workload.dumped_api_call_vec_ptr->emplace_back(std::move(call) );
}

APIInterceptor::APIFunctionArgument Tests::GlCallBuilder::add_chunk(const void*   in_data_ptr,
                                                                   const size_t& in_n_bytes)
{
    const uint32_t chunk_id = m_workload.first_data_chunk_id + static_cast<uint32_t>(m_workload.data_chunk_ptr_vec.size() );

    Capture::APIDumpLoader::DataChunkUniquePtr chunk_ptr(new Capture::APIDumpLoader::DataChunk(in_n_bytes) );

    ::memcpy(chunk_ptr->data(),
             in_data_ptr,
             in_n_bytes);

    m_workload.data_chunk_ptr_vec.push_back(std::move(chunk_ptr) );

    return APIInterceptor::APIFunctionArgument::create_data_chunk_id(chunk_id);
}

void Tests::GlCallBuilder::add_chunk_call(const APIInterceptor::APIFunction& in_function,
                                          const void*                        in_data_ptr,
                                          const size_t&                      in_n_bytes)
{
    const auto chunk_argument = add_chunk(in_data_ptr,
                                          in_n_bytes);

    add_call(in_function,
             {chunk_argument});
}

void Tests::GlCallBuilder::begin(const uint32_t& in_mode)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLBEGIN,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_mode)});
}

void Tests::GlCallBuilder::bind_texture(const uint32_t& in_target,
                                        const uint32_t& in_name)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_target),
              APIInterceptor::APIFunctionArgument::create_u32       (in_name)});
}

void Tests::GlCallBuilder::blend_func(const uint32_t& in_source,
                                       const uint32_t& in_destination)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_source),
              APIInterceptor::APIFunctionArgument::create_u32_glenum(in_destination)});
}

void Tests::GlCallBuilder::call_list(const uint32_t& in_list)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
             {APIInterceptor::APIFunctionArgument::create_u32(in_list)});
}

void Tests::GlCallBuilder::clear(const uint32_t& in_mask)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLCLEAR,
             {APIInterceptor::APIFunctionArgument::create_u32(in_mask)});
}

void Tests::GlCallBuilder::clear_color(const float& in_red,
                                       const float& in_green,
                                       const float& in_blue,
                                       const float& in_alpha)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLCLEARCOLOR,
             {APIInterceptor::APIFunctionArgument::create_fp32(in_red),
              APIInterceptor::APIFunctionArgument::create_fp32(in_green),
              APIInterceptor::APIFunctionArgument::create_fp32(in_blue),
              APIInterceptor::APIFunctionArgument::create_fp32(in_alpha)});
}

void Tests::GlCallBuilder::clear_depth(const double& in_depth)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLCLEARDEPTH,
             {APIInterceptor::APIFunctionArgument::create_fp64(in_depth)});
}

void Tests::GlCallBuilder::clear_stencil(const int32_t& in_stencil)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLCLEARSTENCIL,
             {APIInterceptor::APIFunctionArgument::create_i32(in_stencil)});
}

void Tests::GlCallBuilder::copy_tex_image_2d(const uint32_t& in_level,
                                             const uint32_t& in_internal_format,
                                             const int32_t&  in_x,
                                             const int32_t&  in_y,
                                             const int32_t&  in_width,
                                             const int32_t&  in_height)
{
    constexpr uint32_t GL_TEXTURE_2D = 0x0DE1;

    add_call(APIInterceptor::APIFUNCTION_GL_GLCOPYTEXIMAGE2D,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(GL_TEXTURE_2D),
              APIInterceptor::APIFunctionArgument::create_i32       (static_cast<int32_t>(in_level) ),
              APIInterceptor::APIFunctionArgument::create_u32_glenum(in_internal_format),
              APIInterceptor::APIFunctionArgument::create_i32       (in_x),
              APIInterceptor::APIFunctionArgument::create_i32       (in_y),
              APIInterceptor::APIFunctionArgument::create_i32       (in_width),
              APIInterceptor::APIFunctionArgument::create_i32       (in_height),
              APIInterceptor::APIFunctionArgument::create_i32       (0)});
}

void Tests::GlCallBuilder::copy_tex_sub_image_2d(const uint32_t& in_level,
                                                 const int32_t&  in_x_offset,
                                                 const int32_t&  in_y_offset,
                                                 const int32_t&  in_x,
                                                 const int32_t&  in_y,
                                                 const int32_t&  in_width,
                                                 const int32_t&  in_height)
{
    constexpr uint32_t GL_TEXTURE_2D = 0x0DE1;

    add_call(APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(GL_TEXTURE_2D),
              APIInterceptor::APIFunctionArgument::create_i32       (static_cast<int32_t>(in_level) ),
              APIInterceptor::APIFunctionArgument::create_i32       (in_x_offset),
              APIInterceptor::APIFunctionArgument::create_i32       (in_y_offset),
              APIInterceptor::APIFunctionArgument::create_i32       (in_x),
              APIInterceptor::APIFunctionArgument::create_i32       (in_y),
              APIInterceptor::APIFunctionArgument::create_i32       (in_width),
              APIInterceptor::APIFunctionArgument::create_i32       (in_height)});
}

void Tests::GlCallBuilder::delete_lists(const int32_t& in_first_list,
                                        const int32_t& in_range)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLDELETELISTS,
             {APIInterceptor::APIFunctionArgument::create_i32(in_first_list),
              APIInterceptor::APIFunctionArgument::create_i32(in_range)});
}

void Tests::GlCallBuilder::delete_textures(const std::vector<uint32_t>& in_name_u32vec)
{
    const auto names_arg = add_chunk(in_name_u32vec.data(),
                                     in_name_u32vec.size() * sizeof(uint32_t) );

    add_call(APIInterceptor::APIFUNCTION_GL_GLDELETETEXTURES,
             {APIInterceptor::APIFunctionArgument::create_i32(static_cast<int32_t>(in_name_u32vec.size() ) ),
              names_arg});
}

void Tests::GlCallBuilder::depth_func(const uint32_t& in_function)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_function)});
}

void Tests::GlCallBuilder::depth_mask(const bool& in_is_writing)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK,
             {APIInterceptor::APIFunctionArgument::create_u32((in_is_writing) ? 1
                                                                              : 0)});
}

void Tests::GlCallBuilder::disable(const uint32_t& in_capability)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLDISABLE,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_capability)});
}

void Tests::GlCallBuilder::draw_arrays(const uint32_t&                 in_mode,
                                       const int32_t&                  in_first,
                                       const int32_t&                  in_count,
                                       const std::vector<ClientArray>& in_array_vec)
{
    const auto snapshot_u8vec = make_snapshot(in_array_vec);
    const auto snapshot_arg   = add_chunk(snapshot_u8vec.data(),
                                          snapshot_u8vec.size() );

    add_call(APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_mode),
              APIInterceptor::APIFunctionArgument::create_i32       (in_first),
              APIInterceptor::APIFunctionArgument::create_i32       (in_count),
              snapshot_arg});
}

void Tests::GlCallBuilder::draw_elements(const uint32_t&                 in_mode,
                                         const std::vector<uint16_t>&    in_index_u16vec,
                                         const std::vector<ClientArray>& in_array_vec)
{
    constexpr uint32_t GL_UNSIGNED_SHORT = 0x1403;

    const auto snapshot_u8vec = make_snapshot(in_array_vec);
    const auto indices_arg    = add_chunk(in_index_u16vec.data(),
                                          in_index_u16vec.size() * sizeof(uint16_t) );
    const auto snapshot_arg   = add_chunk(snapshot_u8vec.data(),
                                          snapshot_u8vec.size() );

    add_call(APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_mode),
              APIInterceptor::APIFunctionArgument::create_i32       (static_cast<int32_t>(in_index_u16vec.size() ) ),
              APIInterceptor::APIFunctionArgument::create_u32_glenum(GL_UNSIGNED_SHORT),
              indices_arg,
              snapshot_arg});
}

void Tests::GlCallBuilder::enable(const uint32_t& in_capability)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_capability)});
}

void Tests::GlCallBuilder::end()
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLEND,
             {});
}

void Tests::GlCallBuilder::end_list()
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST,
             {});
}

void Tests::GlCallBuilder::frustum(const double& in_left,
                                   const double& in_right,
                                   const double& in_bottom,
                                   const double& in_top,
                                   const double& in_near,
                                   const double& in_far)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLFRUSTUM,
             {APIInterceptor::APIFunctionArgument::create_fp64(in_left),
              APIInterceptor::APIFunctionArgument::create_fp64(in_right),
              APIInterceptor::APIFunctionArgument::create_fp64(in_bottom),
              APIInterceptor::APIFunctionArgument::create_fp64(in_top),
              APIInterceptor::APIFunctionArgument::create_fp64(in_near),
              APIInterceptor::APIFunctionArgument::create_fp64(in_far)});
}

const Capture::APIDumpLoader::Workload& Tests::GlCallBuilder::get_workload() const
{
    return m_workload;
}

void Tests::GlCallBuilder::line_width(const float& in_width)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLLINEWIDTH,
             {APIInterceptor::APIFunctionArgument::create_fp32(in_width)});
}

void Tests::GlCallBuilder::load_identity()
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY,
             {});
}

void Tests::GlCallBuilder::load_matrix(const std::array<float, 16>& in_matrix_f32mat)
{
    add_chunk_call(APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF,
                   in_matrix_f32mat.data(),
                   sizeof(in_matrix_f32mat) );
}

void Tests::GlCallBuilder::load_matrix_double(const std::array<double, 16>& in_matrix_d64mat)
{
    add_chunk_call(APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD,
                   in_matrix_d64mat.data(),
                   sizeof(in_matrix_d64mat) );
}

void Tests::GlCallBuilder::make_current(const uint64_t& in_device_context_address,
                                        const uint64_t& in_context_address,
                                        const bool&     in_succeeds)
{
    add_call(APIInterceptor::APIFUNCTION_WGL_WGLMAKECURRENT,
             {make_pointer_argument(in_device_context_address),
              make_pointer_argument(in_context_address)},
             APIInterceptor::APIFunctionArgument::create_i32((in_succeeds) ? 1
                                                                           : 0) );
}

APIInterceptor::APIFunctionArgument Tests::GlCallBuilder::make_pointer_argument(const uint64_t& in_address)
{
    return APIInterceptor::APIFunctionArgument::create_void_ptr(reinterpret_cast<void*>(static_cast<uintptr_t>(in_address) ));
}

std::vector<uint8_t> Tests::GlCallBuilder::make_snapshot(const std::vector<ClientArray>& in_array_vec)
{
    std::vector<uint8_t> result_u8vec;

    /* The fields of an array, in the order in which the dumper writes them (see ClientArrayManager). */
    const auto add_u32_func = [&result_u8vec](const uint32_t& in_value)
    {
        const uint8_t* value_ptr = reinterpret_cast<const uint8_t*>(&in_value);

        result_u8vec.insert(result_u8vec.end(),
                            value_ptr,
                            value_ptr + sizeof(in_value) );
    };

    add_u32_func(static_cast<uint32_t>(in_array_vec.size() ) );

    for (const auto& array : in_array_vec)
    {
        const uint64_t pointer = 0x1000;

        add_u32_func(array.kind);
        add_u32_func(0);
        add_u32_func(static_cast<uint32_t>(array.size) );
        add_u32_func(array.type);
        add_u32_func(array.stride);
        add_u32_func(0);
        add_u32_func(0);

        result_u8vec.insert(result_u8vec.end(),
                            reinterpret_cast<const uint8_t*>(&pointer),
                            reinterpret_cast<const uint8_t*>(&pointer) + sizeof(pointer) );

        add_u32_func(array.first_element);
        add_u32_func(array.n_elements);
        add_u32_func(static_cast<uint32_t>(array.data_u8vec.size() ) );

        result_u8vec.insert(result_u8vec.end(),
                            array.data_u8vec.begin(),
                            array.data_u8vec.end() );
    }

    return result_u8vec;
}

Tests::GlCallBuilder::ClientArray Tests::GlCallBuilder::make_vertex_array(const std::vector<std::array<float, 3> >& in_position_vec)
{
    ClientArray result;

    result.n_elements = static_cast<uint32_t>(in_position_vec.size() );

    result.data_u8vec.resize(in_position_vec.size() * sizeof(std::array<float, 3>) );

    if (in_position_vec.empty() == false)
    {
        ::memcpy(result.data_u8vec.data(),
                 in_position_vec.data(),
                 result.data_u8vec.size() );
    }

    return result;
}

void Tests::GlCallBuilder::matrix_mode(const uint32_t& in_mode)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_mode)});
}

void Tests::GlCallBuilder::mult_matrix(const std::array<float, 16>& in_matrix_f32mat)
{
    add_chunk_call(APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF,
                   in_matrix_f32mat.data(),
                   sizeof(in_matrix_f32mat) );
}

void Tests::GlCallBuilder::new_list(const uint32_t& in_list,
                                     const uint32_t& in_mode)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
             {APIInterceptor::APIFunctionArgument::create_u32(in_list),
              APIInterceptor::APIFunctionArgument::create_u32_glenum(in_mode)});
}

void Tests::GlCallBuilder::ortho(const double& in_left,
                                 const double& in_right,
                                 const double& in_bottom,
                                 const double& in_top,
                                 const double& in_near,
                                 const double& in_far)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLORTHO,
             {APIInterceptor::APIFunctionArgument::create_fp64(in_left),
              APIInterceptor::APIFunctionArgument::create_fp64(in_right),
              APIInterceptor::APIFunctionArgument::create_fp64(in_bottom),
              APIInterceptor::APIFunctionArgument::create_fp64(in_top),
              APIInterceptor::APIFunctionArgument::create_fp64(in_near),
              APIInterceptor::APIFunctionArgument::create_fp64(in_far)});
}

void Tests::GlCallBuilder::point_size(const float& in_size)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLPOINTSIZE,
             {APIInterceptor::APIFunctionArgument::create_fp32(in_size)});
}

void Tests::GlCallBuilder::pop_attrib()
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB,
             {});
}

void Tests::GlCallBuilder::pop_matrix()
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLPOPMATRIX,
             {});
}

void Tests::GlCallBuilder::push_attrib(const uint32_t& in_mask)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB,
             {APIInterceptor::APIFunctionArgument::create_u32(in_mask)});
}

void Tests::GlCallBuilder::push_matrix()
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLPUSHMATRIX,
             {});
}

void Tests::GlCallBuilder::read_pixels(const int32_t& in_x,
                                       const int32_t& in_y,
                                       const int32_t& in_width,
                                       const int32_t& in_height)
{
    constexpr uint32_t GL_RGB           = 0x1907;
    constexpr uint32_t GL_UNSIGNED_BYTE = 0x1401;

    add_call(APIInterceptor::APIFUNCTION_GL_GLREADPIXELS,
             {APIInterceptor::APIFunctionArgument::create_i32       (in_x),
              APIInterceptor::APIFunctionArgument::create_i32       (in_y),
              APIInterceptor::APIFunctionArgument::create_i32       (in_width),
              APIInterceptor::APIFunctionArgument::create_i32       (in_height),
              APIInterceptor::APIFunctionArgument::create_u32_glenum(GL_RGB),
              APIInterceptor::APIFunctionArgument::create_u32_glenum(GL_UNSIGNED_BYTE),
              make_pointer_argument(0)});
}

void Tests::GlCallBuilder::rotate(const float& in_angle_degrees,
                                  const float& in_x,
                                  const float& in_y,
                                  const float& in_z)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLROTATEF,
             {APIInterceptor::APIFunctionArgument::create_fp32(in_angle_degrees),
              APIInterceptor::APIFunctionArgument::create_fp32(in_x),
              APIInterceptor::APIFunctionArgument::create_fp32(in_y),
              APIInterceptor::APIFunctionArgument::create_fp32(in_z)});
}

void Tests::GlCallBuilder::scale(const float& in_x,
                                 const float& in_y,
                                 const float& in_z)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLSCALEF,
             {APIInterceptor::APIFunctionArgument::create_fp32(in_x),
              APIInterceptor::APIFunctionArgument::create_fp32(in_y),
              APIInterceptor::APIFunctionArgument::create_fp32(in_z)});
}

void Tests::GlCallBuilder::scissor(const int32_t& in_x,
                                   const int32_t& in_y,
                                   const int32_t& in_width,
                                   const int32_t& in_height)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLSCISSOR,
             {APIInterceptor::APIFunctionArgument::create_i32(in_x),
              APIInterceptor::APIFunctionArgument::create_i32(in_y),
              APIInterceptor::APIFunctionArgument::create_i32(in_width),
              APIInterceptor::APIFunctionArgument::create_i32(in_height)});
}

void Tests::GlCallBuilder::set_thread_id(const uint32_t& in_thread_id)
{
    m_thread_id = in_thread_id;
}

void Tests::GlCallBuilder::swap_buffers(const uint64_t& in_device_context_address)
{
    add_call(APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS,
             {make_pointer_argument(in_device_context_address)},
             APIInterceptor::APIFunctionArgument::create_i32(1) );
}

void Tests::GlCallBuilder::tex_image_2d(const uint32_t&             in_level,
                                        const int32_t&              in_width,
                                        const int32_t&              in_height,
                                        const std::vector<uint8_t>& in_pixel_u8vec)
{
    constexpr uint32_t GL_RGBA             = 0x1908;
    constexpr uint32_t GL_TEXTURE_2D       = 0x0DE1;
    constexpr uint32_t GL_UNSIGNED_BYTE    = 0x1401;
    constexpr int32_t  INTERNAL_FORMAT_RGBA = 4;

    APIInterceptor::APIFunctionArgument pixels_arg = make_pointer_argument(0);

    if (in_pixel_u8vec.empty() == false)
    {
        pixels_arg = add_chunk(in_pixel_u8vec.data(),
                               in_pixel_u8vec.size() );
    }

    add_call(APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(GL_TEXTURE_2D),
              APIInterceptor::APIFunctionArgument::create_i32       (static_cast<int32_t>(in_level) ),
              APIInterceptor::APIFunctionArgument::create_i32       (INTERNAL_FORMAT_RGBA),
              APIInterceptor::APIFunctionArgument::create_i32       (in_width),
              APIInterceptor::APIFunctionArgument::create_i32       (in_height),
              APIInterceptor::APIFunctionArgument::create_i32       (0),
              APIInterceptor::APIFunctionArgument::create_u32_glenum(GL_RGBA),
              APIInterceptor::APIFunctionArgument::create_u32_glenum(GL_UNSIGNED_BYTE),
              pixels_arg});
}

void Tests::GlCallBuilder::translate(const double& in_x,
                                     const double& in_y,
                                     const double& in_z)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLTRANSLATED,
             {APIInterceptor::APIFunctionArgument::create_fp64(in_x),
              APIInterceptor::APIFunctionArgument::create_fp64(in_y),
              APIInterceptor::APIFunctionArgument::create_fp64(in_z)});
}

void Tests::GlCallBuilder::vertex2f(const float& in_x,
                                    const float& in_y)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX2F,
             {APIInterceptor::APIFunctionArgument::create_fp32(in_x),
              APIInterceptor::APIFunctionArgument::create_fp32(in_y)});
}

void Tests::GlCallBuilder::vertex3f(const float& in_x,
                                    const float& in_y,
                                    const float& in_z)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3F,
             {APIInterceptor::APIFunctionArgument::create_fp32(in_x),
              APIInterceptor::APIFunctionArgument::create_fp32(in_y),
              APIInterceptor::APIFunctionArgument::create_fp32(in_z)});
}

void Tests::GlCallBuilder::vertex3fv(const std::array<float, 3>& in_position)
{
    add_chunk_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV,
                   in_position.data(),
                   sizeof(in_position) );
}

void Tests::GlCallBuilder::viewport(const int32_t& in_x,
                                    const int32_t& in_y,
                                    const int32_t& in_width,
                                    const int32_t& in_height)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLVIEWPORT,
             {APIInterceptor::APIFunctionArgument::create_i32(in_x),
              APIInterceptor::APIFunctionArgument::create_i32(in_y),
              APIInterceptor::APIFunctionArgument::create_i32(in_width),
              APIInterceptor::APIFunctionArgument::create_i32(in_height)});
}

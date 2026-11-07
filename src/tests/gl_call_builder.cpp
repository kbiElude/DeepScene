/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_call_builder.h"
#include "common/types.h"

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

void Tests::GlCallBuilder::begin(const uint32_t& in_mode)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLBEGIN,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_mode)});
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

void Tests::GlCallBuilder::disable(const uint32_t& in_capability)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLDISABLE,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_capability)});
}

void Tests::GlCallBuilder::enable(const uint32_t& in_capability)
{
    add_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,
             {APIInterceptor::APIFunctionArgument::create_u32_glenum(in_capability)});
}

const Capture::APIDumpLoader::Workload& Tests::GlCallBuilder::get_workload() const
{
    return m_workload;
}

APIInterceptor::APIFunctionArgument Tests::GlCallBuilder::make_pointer_argument(const uint64_t& in_address)
{
    return APIInterceptor::APIFunctionArgument::create_void_ptr(reinterpret_cast<void*>(static_cast<uintptr_t>(in_address) ));
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

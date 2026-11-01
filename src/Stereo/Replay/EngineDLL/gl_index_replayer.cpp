/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_index_replayer.h"
#include <string.h>
#include <Windows.h>
#include "Khronos/GL/glcorearb.h"

Stereo::Replay::EngineDLL::GlIndexReplayer::GlIndexReplayer(GlReplayContext* in_context_ptr,
                                                            ResolveFunc      in_resolve_func)
    :m_context_ptr (in_context_ptr),
     m_resolve_func(in_resolve_func)
{
    /* Stub */
}

std::vector<uint32_t> Stereo::Replay::EngineDLL::GlIndexReplayer::get_numbers(const GlReplayContext& in_context,
                                                                              const size_t&          in_n_argument)
{
    size_t                n_bytes  = 0;
    const uint8_t*        data_ptr = in_context.get_chunk_data_ptr(in_context.get_in_argument_ptr(in_n_argument),
                                                                   &n_bytes);
    std::vector<uint32_t> result_vec(n_bytes / sizeof(uint32_t));

    if (data_ptr != nullptr)
    {
        ::memcpy(result_vec.data(),
                 data_ptr,
                 result_vec.size() * sizeof(uint32_t) );
    }

    return result_vec;
}

bool Stereo::Replay::EngineDLL::GlIndexReplayer::is_index_function(const APIInterceptor::APIFunction& in_function)
{
    return (in_function == APIInterceptor::APIFUNCTION_GL_GLDRAWBUFFERS                 ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLNAMEDFRAMEBUFFERDRAWBUFFERS ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLUNIFORMSUBROUTINESUIV);
}

std::vector<uint32_t> Stereo::Replay::EngineDLL::GlIndexReplayer::permute(const std::vector<uint32_t>& in_dumped_vec,
                                                                          const ObjectKind&            in_kind,
                                                                          const uint32_t&              in_scope,
                                                                          const uint32_t&              in_empty_value) const
{
    std::vector<bool> is_taken(in_dumped_vec.size(),
                               false);

    const uint64_t        program = m_context_ptr->get_current_program();
    std::vector<uint32_t> result_vec(in_dumped_vec.size(),
                                     in_empty_value);

    /* The elements whose number differs go where the replay's number says. */
    for (size_t n_element = 0;
                n_element < in_dumped_vec.size();
              ++n_element)
    {
        const uint32_t live_position = m_context_ptr->map_index(in_kind,
                                                                program,
                                                                in_scope,
                                                                static_cast<uint32_t>(n_element) );

        if (live_position != n_element            &&
            live_position <  in_dumped_vec.size() )
        {
            result_vec.at(live_position) = in_dumped_vec.at(n_element);
            is_taken  [live_position]    = true;
        }
    }

    /* The others stay where they are, or take the first place which is free. */
    for (size_t n_element = 0;
                n_element < in_dumped_vec.size();
              ++n_element)
    {
        const uint32_t live_position = m_context_ptr->map_index(in_kind,
                                                                program,
                                                                in_scope,
                                                                static_cast<uint32_t>(n_element) );

        if (live_position == n_element            ||
            live_position >= in_dumped_vec.size() )
        {
            size_t n_position = n_element;

            while (n_position < in_dumped_vec.size() &&
                   is_taken[n_position])
            {
                ++n_position;
            }

            if (n_position < in_dumped_vec.size() )
            {
                result_vec.at(n_position) = in_dumped_vec.at(n_element);
                is_taken  [n_position]    = true;
            }
        }
    }

    return result_vec;
}

bool Stereo::Replay::EngineDLL::GlIndexReplayer::replay(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                        std::string*                                 out_error_ptr)
{
    bool result = false;

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLDRAWBUFFERS:
        {
            result = replay_draw_buffers(in_call,
                                         false, /* in_is_named */
                                         out_error_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLNAMEDFRAMEBUFFERDRAWBUFFERS:
        {
            result = replay_draw_buffers(in_call,
                                         true, /* in_is_named */
                                         out_error_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLUNIFORMSUBROUTINESUIV:
        {
            result = replay_uniform_subroutines(in_call,
                                                out_error_ptr);

            break;
        }

        default:
        {
            *out_error_ptr = "Not a function with an array of indices.";

            break;
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::GlIndexReplayer::replay_draw_buffers(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                     const bool&                                  in_is_named,
                                                                     std::string*                                 out_error_ptr)
{
    typedef void (APIENTRY* PFNDRAWBUFFERSPROC)     (GLsizei, const GLenum*);
    typedef void (APIENTRY* PFNNAMEDDRAWBUFFERSPROC)(GLuint, GLsizei, const GLenum*);

    const size_t n_first_argument = (in_is_named) ? 1 : 0;
    uint32_t     framebuffer      = 0;
    bool         result           = false;

    if (in_call.arg_in_vec.size() < n_first_argument + 2)
    {
        *out_error_ptr = "A call that sets the draw buffers has too few arguments.";

        goto end;
    }

    {
        /* Position n of the array is the n-th output of the fragment shader; the rest of the places hold nothing. */
        const std::vector<uint32_t> live_vec = permute(get_numbers(*m_context_ptr,
                                                                   n_first_argument + 1),
                                                       ObjectKind::FRAG_DATA_LOCATION,
                                                       0, /* in_scope */
                                                       GL_NONE);

        if (in_is_named)
        {
            const auto draw_func_ptr = reinterpret_cast<PFNNAMEDDRAWBUFFERSPROC>(m_resolve_func("glNamedFramebufferDrawBuffers") );

            read_number(in_call.arg_in_vec.at(0),
                       &framebuffer);

            if (draw_func_ptr != nullptr)
            {
                draw_func_ptr(m_context_ptr->map_name        (ObjectKind::FRAMEBUFFER,
                                                              framebuffer),
                              static_cast<GLsizei>           (live_vec.size() ),
                              reinterpret_cast<const GLenum*>(live_vec.data() ));
            }
        }
        else
        {
            const auto draw_func_ptr = reinterpret_cast<PFNDRAWBUFFERSPROC>(m_resolve_func("glDrawBuffers") );

            if (draw_func_ptr != nullptr)
            {
                draw_func_ptr(static_cast<GLsizei>           (live_vec.size() ),
                              reinterpret_cast<const GLenum*>(live_vec.data() ));
            }
        }
    }

    result = true;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::GlIndexReplayer::replay_uniform_subroutines(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                            std::string*                                 out_error_ptr)
{
    typedef void (APIENTRY* PFNSUBROUTINESPROC)(GLenum, GLsizei, const GLuint*);

    bool     result      = false;
    uint32_t shader_type = 0;

    if (in_call.arg_in_vec.size() < 3)
    {
        *out_error_ptr = "A call that sets the subroutines has too few arguments.";

        goto end;
    }

    read_number(in_call.arg_in_vec.at(0),
               &shader_type);

    {
        std::vector<uint32_t> dumped_vec = get_numbers(*m_context_ptr,
                                                       2);

        /* The numbers of the subroutines are the replay's own. */
        for (auto& dumped_index : dumped_vec)
        {
            dumped_index = m_context_ptr->map_index(ObjectKind::SUBROUTINE_INDEX,
                                                    m_context_ptr->get_current_program(),
                                                    shader_type,
                                                    dumped_index);
        }

        const std::vector<uint32_t> live_vec         = permute                             (dumped_vec,
                                                                                            ObjectKind::SUBROUTINE_UNIFORM_LOCATION,
                                                                                            shader_type,
                                                                                            0);
        const auto                  uniform_func_ptr = reinterpret_cast<PFNSUBROUTINESPROC>(m_resolve_func("glUniformSubroutinesuiv") );

        if (uniform_func_ptr != nullptr &&
            live_vec.empty() == false)
        {
            uniform_func_ptr(shader_type,
                             static_cast<GLsizei>(live_vec.size() ),
                             live_vec.data() );
        }
    }

    result = true;

end:
    return result;
}

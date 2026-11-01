/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_mapping_replayer.h"
#include <string.h>
#include <Windows.h>
#include "Khronos/GL/glcorearb.h"

Replay::StereoReplayEngineDLL::GlMappingReplayer::GlMappingReplayer(GlReplayContext* in_context_ptr,
                                                                    ResolveFunc      in_resolve_func)
    :m_context_ptr (in_context_ptr),
     m_resolve_func(in_resolve_func)
{
    /* Stub */
}

bool Replay::StereoReplayEngineDLL::GlMappingReplayer::flush(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                      const bool&                         in_is_named,
                                                                      std::string*                        out_error_ptr)
{
    constexpr size_t N_ARGUMENT_DATA   = 3;
    constexpr size_t N_ARGUMENT_LENGTH = 2;
    constexpr size_t N_ARGUMENT_OFFSET = 1;
    constexpr size_t N_ARGUMENTS       = N_ARGUMENT_DATA + 1;

    typedef void (APIENTRY* PFNFLUSHPROC)     (GLenum, GLintptr, GLsizeiptr);
    typedef void (APIENTRY* PFNFLUSHNAMEDPROC)(GLuint, GLintptr, GLsizeiptr);

    const auto iterator = m_mapping_map.find(get_mapping_key(in_call,
                                                             in_is_named) );
    int64_t    length   = 0;
    int64_t    offset   = 0;
    bool       result   = false;

    if (in_call.arg_in_vec.size() < N_ARGUMENTS)
    {
        *out_error_ptr = "A flush of a mapped buffer has too few arguments.";

        goto end;
    }

    read_number(in_call.arg_in_vec.at(N_ARGUMENT_OFFSET),
               &offset);
    read_number(in_call.arg_in_vec.at(N_ARGUMENT_LENGTH),
               &length);

    if (iterator != m_mapping_map.end() )
    {
        size_t         n_bytes  = 0;
        const uint8_t* data_ptr = m_context_ptr->get_chunk_data_ptr(&in_call.arg_in_vec.at(N_ARGUMENT_DATA),
                                                                    &n_bytes);

        if (data_ptr != nullptr    &&
            offset   >= 0          &&
            offset   <= iterator->second.n_bytes)
        {
            const int64_t n_available = iterator->second.n_bytes - offset;
            const size_t  n_to_copy   = (static_cast<int64_t>(n_bytes) < n_available) ? n_bytes
                                                                                      : static_cast<size_t>(n_available);

            ::memcpy(iterator->second.live_data_ptr + offset,
                     data_ptr,
                     n_to_copy);
        }

        if (in_is_named)
        {
            const auto flush_func_ptr = reinterpret_cast<PFNFLUSHNAMEDPROC>(m_resolve_func("glFlushMappedNamedBufferRange") );

            if (flush_func_ptr != nullptr)
            {
                flush_func_ptr(m_context_ptr->map_name(ObjectKind::BUFFER,
                                                       get_first_argument(in_call)),
                               static_cast<GLintptr>(offset),
                               static_cast<GLsizeiptr>(length) );
            }
        }
        else
        {
            const auto flush_func_ptr = reinterpret_cast<PFNFLUSHPROC>(m_resolve_func("glFlushMappedBufferRange") );

            if (flush_func_ptr != nullptr)
            {
                flush_func_ptr(get_first_argument(in_call),
                               static_cast<GLintptr>(offset),
                               static_cast<GLsizeiptr>(length) );
            }
        }
    }

    result = true;

end:
    return result;
}

uint32_t Replay::StereoReplayEngineDLL::GlMappingReplayer::get_first_argument(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    uint32_t result = 0;

    if (in_call.arg_in_vec.empty() == false)
    {
        read_number(in_call.arg_in_vec.at(0),
                   &result);
    }

    return result;
}

uint64_t Replay::StereoReplayEngineDLL::GlMappingReplayer::get_mapping_key(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                    const bool&                         in_is_named) const
{
    /* The mappings of named buffers are kept apart from those of the binding points. */
    constexpr uint64_t NAMED_BUFFER_KEY_FLAG = 0x100000000ull;

    uint64_t result = get_first_argument(in_call);

    if (in_is_named)
    {
        result = NAMED_BUFFER_KEY_FLAG | m_context_ptr->map_name(ObjectKind::BUFFER,
                                                                  static_cast<uint32_t>(result) );
    }

    return result;
}

bool Replay::StereoReplayEngineDLL::GlMappingReplayer::is_mapping_function(const APIInterceptor::APIFunction& in_function)
{
    return (in_function == APIInterceptor::APIFUNCTION_GL_GLFLUSHMAPPEDBUFFERRANGE      ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLFLUSHMAPPEDNAMEDBUFFERRANGE ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLMAPBUFFER                   ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLMAPBUFFERRANGE              ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLMAPNAMEDBUFFER              ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLMAPNAMEDBUFFERRANGE         ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLUNMAPBUFFER                 ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLUNMAPNAMEDBUFFER);
}

bool Replay::StereoReplayEngineDLL::GlMappingReplayer::map(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                    const bool&                         in_is_named,
                                                                    const bool&                         in_is_range,
                                                                    std::string*                        out_error_ptr)
{
    constexpr size_t N_ARGUMENT_ACCESS         = 1;
    constexpr size_t N_ARGUMENT_LENGTH         = 2;
    constexpr size_t N_ARGUMENT_OFFSET         = 1;
    constexpr size_t N_ARGUMENT_RANGE_ACCESS   = 3;
    constexpr size_t N_ARGUMENT_TARGET         = 0;
    constexpr size_t N_ARGUMENTS_RANGE         = N_ARGUMENT_RANGE_ACCESS + 1;
    constexpr size_t N_ARGUMENTS_WITHOUT_RANGE = N_ARGUMENT_ACCESS + 1;

    typedef void* (APIENTRY* PFNMAPPROC)          (GLenum, GLenum);
    typedef void* (APIENTRY* PFNMAPNAMEDPROC)     (GLuint, GLenum);
    typedef void* (APIENTRY* PFNMAPNAMEDRANGEPROC)(GLuint, GLintptr, GLsizeiptr, GLbitfield);
    typedef void* (APIENTRY* PFNMAPRANGEPROC)     (GLenum, GLintptr, GLsizeiptr, GLbitfield);
    typedef void  (APIENTRY* PFNGETNAMEDPARAMPROC)(GLuint, GLenum, GLint*);
    typedef void  (APIENTRY* PFNGETPARAMPROC)     (GLenum, GLenum, GLint*);

    uint32_t access           = 0;
    GLint    buffer_size      = 0;
    void*    data_ptr         = nullptr;
    int64_t  length           = 0;
    int64_t  offset           = 0;
    bool     result           = false;
    uint32_t target_or_buffer = 0;

    if (in_call.arg_in_vec.size() < ((in_is_range) ? N_ARGUMENTS_RANGE : N_ARGUMENTS_WITHOUT_RANGE) )
    {
        *out_error_ptr = "A mapping of a buffer has too few arguments.";

        goto end;
    }

    read_number(in_call.arg_in_vec.at(N_ARGUMENT_TARGET),
               &target_or_buffer);

    if (in_is_range)
    {
        read_number(in_call.arg_in_vec.at(N_ARGUMENT_OFFSET),
                   &offset);
        read_number(in_call.arg_in_vec.at(N_ARGUMENT_LENGTH),
                   &length);
        read_number(in_call.arg_in_vec.at(N_ARGUMENT_RANGE_ACCESS),
                   &access);
    }
    else
    {
        read_number(in_call.arg_in_vec.at(N_ARGUMENT_ACCESS),
                   &access);
    }

    if (in_is_named)
    {
        const GLuint buffer = m_context_ptr->map_name(ObjectKind::BUFFER,
                                                      target_or_buffer);

        if (in_is_range == false)
        {
            const auto get_func_ptr = reinterpret_cast<PFNGETNAMEDPARAMPROC>(m_resolve_func("glGetNamedBufferParameteriv") );

            if (get_func_ptr != nullptr)
            {
                get_func_ptr(buffer,
                             GL_BUFFER_SIZE,
                            &buffer_size);
            }

            length = buffer_size;

            const auto map_func_ptr = reinterpret_cast<PFNMAPNAMEDPROC>(m_resolve_func("glMapNamedBuffer") );

            data_ptr = (map_func_ptr != nullptr) ? map_func_ptr(buffer,
                                                                access)
                                            : nullptr;
        }
        else
        {
            const auto map_func_ptr = reinterpret_cast<PFNMAPNAMEDRANGEPROC>(m_resolve_func("glMapNamedBufferRange") );

            data_ptr = (map_func_ptr != nullptr) ? map_func_ptr(buffer,
                                                                static_cast<GLintptr>(offset),
                                                                static_cast<GLsizeiptr>(length),
                                                                access)
                                            : nullptr;
        }
    }
    else
    {
        if (in_is_range == false)
        {
            const auto get_func_ptr = reinterpret_cast<PFNGETPARAMPROC>(m_resolve_func("glGetBufferParameteriv") );
            const auto map_func_ptr = reinterpret_cast<PFNMAPPROC>(m_resolve_func("glMapBuffer") );

            if (get_func_ptr != nullptr)
            {
                get_func_ptr(target_or_buffer,
                             GL_BUFFER_SIZE,
                            &buffer_size);
            }

            length   = buffer_size;
            data_ptr = (map_func_ptr != nullptr) ? map_func_ptr(target_or_buffer,
                                                                access)
                                            : nullptr;
        }
        else
        {
            const auto map_func_ptr = reinterpret_cast<PFNMAPRANGEPROC>(m_resolve_func("glMapBufferRange") );

            data_ptr = (map_func_ptr != nullptr) ? map_func_ptr(target_or_buffer,
                                                                static_cast<GLintptr>(offset),
                                                                static_cast<GLsizeiptr>(length),
                                                                access)
                                            : nullptr;
        }
    }

    /* A mapping which failed holds no data: the dump's writes have nowhere to go. */
    if (data_ptr != nullptr)
    {
        Mapping mapping;

        mapping.live_data_ptr = static_cast<uint8_t*>(data_ptr);
        mapping.n_bytes       = length;

        m_mapping_map[get_mapping_key(in_call,
                                      in_is_named)] = mapping;
    }

    result = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::GlMappingReplayer::replay(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       std::string*                        out_error_ptr)
{
    bool result = false;

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLFLUSHMAPPEDBUFFERRANGE:
        {
            result = flush(in_call,
                           false, /* in_is_named */
                           out_error_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLFLUSHMAPPEDNAMEDBUFFERRANGE:
        {
            result = flush(in_call,
                           true, /* in_is_named */
                           out_error_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLMAPBUFFER:
        {
            result = map(in_call,
                         false, /* in_is_named */
                         false, /* in_is_range */
                         out_error_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLMAPBUFFERRANGE:
        {
            result = map(in_call,
                         false, /* in_is_named */
                         true,  /* in_is_range */
                         out_error_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLMAPNAMEDBUFFER:
        {
            result = map(in_call,
                         true,  /* in_is_named */
                         false, /* in_is_range */
                         out_error_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLMAPNAMEDBUFFERRANGE:
        {
            result = map(in_call,
                         true, /* in_is_named */
                         true, /* in_is_range */
                         out_error_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLUNMAPBUFFER:
        {
            result = unmap(in_call,
                           false, /* in_is_named */
                           out_error_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLUNMAPNAMEDBUFFER:
        {
            result = unmap(in_call,
                           true, /* in_is_named */
                           out_error_ptr);

            break;
        }

        default:
        {
            *out_error_ptr = "Not a mapping function.";

            break;
        }
    }

    return result;
}

void Replay::StereoReplayEngineDLL::GlMappingReplayer::reset()
{
    m_mapping_map.clear();
}

bool Replay::StereoReplayEngineDLL::GlMappingReplayer::unmap(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                      const bool&                         in_is_named,
                                                                      std::string*                        out_error_ptr)
{
    typedef GLboolean (APIENTRY* PFNUNMAPPROC)     (GLenum);
    typedef GLboolean (APIENTRY* PFNUNMAPNAMEDPROC)(GLuint);

    const auto iterator = m_mapping_map.find(get_mapping_key(in_call,
                                                             in_is_named) );
    bool       result   = false;

    if (in_call.arg_in_vec.empty() )
    {
        *out_error_ptr = "An unmap of a buffer has no arguments.";

        goto end;
    }

    if (iterator != m_mapping_map.end() )
    {
        /* What the app wrote is the last argument after the call. */
        size_t         n_bytes  = 0;
        const uint8_t* data_ptr = (in_call.arg_out_vec.empty() ) ? nullptr
                                                                 : m_context_ptr->get_chunk_data_ptr(&in_call.arg_out_vec.back(),
                                                                                                     &n_bytes);

        if (data_ptr != nullptr)
        {
            ::memcpy(iterator->second.live_data_ptr,
                     data_ptr,
                     (static_cast<int64_t>(n_bytes) < iterator->second.n_bytes) ? n_bytes
                                                                                : static_cast<size_t>(iterator->second.n_bytes) );
        }

        m_mapping_map.erase(iterator);
    }

    if (in_is_named)
    {
        const auto unmap_func_ptr = reinterpret_cast<PFNUNMAPNAMEDPROC>(m_resolve_func("glUnmapNamedBuffer") );

        if (unmap_func_ptr != nullptr)
        {
            unmap_func_ptr(m_context_ptr->map_name(ObjectKind::BUFFER,
                                                   get_first_argument(in_call)));
        }
    }
    else
    {
        const auto unmap_func_ptr = reinterpret_cast<PFNUNMAPPROC>(m_resolve_func("glUnmapBuffer") );

        if (unmap_func_ptr != nullptr)
        {
            unmap_func_ptr(get_first_argument(in_call));
        }
    }

    result = true;

end:
    return result;
}

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_client_array_replayer.h"
#include <string.h>
#include <Windows.h>
#include "Common/macros.h"
#include "Khronos/GL/glcorearb.h"

Replay::StereoReplayEngineDLL::GlClientArrayReplayer::GlClientArrayReplayer(GlReplayContext* in_context_ptr,
                                                                            ResolveFunc      in_resolve_func)
    :m_context_ptr (in_context_ptr),
     m_resolve_func(in_resolve_func)
{
    /* Stub */
}

bool Replay::StereoReplayEngineDLL::GlClientArrayReplayer::apply(const uint8_t* in_snapshot_ptr,
                                                                 const size_t&  in_n_bytes)
{
    /* Compatibility-profile token which the core headers do not have. */
    constexpr GLenum COMPAT_GL_CLIENT_ACTIVE_TEXTURE = 0x84E1;

    typedef void (APIENTRY* PFNBINDBUFFERPROC)         (GLenum, GLuint);
    typedef void (APIENTRY* PFNCLIENTACTIVETEXTUREPROC)(GLenum);
    typedef void (APIENTRY* PFNGETINTEGERVPROC)        (GLenum, GLint*);

    const auto bind_buffer_func_ptr   = reinterpret_cast<PFNBINDBUFFERPROC>         (m_resolve_func("glBindBuffer") );
    const auto client_active_func_ptr = reinterpret_cast<PFNCLIENTACTIVETEXTUREPROC>(m_resolve_func("glClientActiveTexture") );
    const auto get_integer_func_ptr   = reinterpret_cast<PFNGETINTEGERVPROC>        (m_resolve_func("glGetIntegerv") );
    uint32_t   n_arrays               = 0;
    size_t     offset                 = sizeof(n_arrays);
    bool       result                 = false;
    GLint      saved_array_buffer     = 0;
    GLint      saved_client_active    = GL_TEXTURE0;

    if (in_snapshot_ptr      == nullptr          ||
        bind_buffer_func_ptr == nullptr          ||
        get_integer_func_ptr == nullptr          ||
        in_n_bytes           <  sizeof(n_arrays) )
    {
        goto end;
    }

    ::memcpy(&n_arrays,
             in_snapshot_ptr,
             sizeof(n_arrays) );

    /* The arrays read the memory only if no buffer is bound at the time of the call that points them. */
    get_integer_func_ptr(GL_ARRAY_BUFFER_BINDING,
                        &saved_array_buffer);
    get_integer_func_ptr(COMPAT_GL_CLIENT_ACTIVE_TEXTURE,
                        &saved_client_active);
    bind_buffer_func_ptr(GL_ARRAY_BUFFER,
                         0);

    for (uint32_t n_array = 0;
                  n_array < n_arrays;
                ++n_array)
    {
        SnapshotArray array = {};

        if (offset + sizeof(array) > in_n_bytes)
        {
            break;
        }

        ::memcpy(&array,
                 in_snapshot_ptr + offset,
                 sizeof(array) );

        offset += sizeof(array);

        if (offset + array.n_bytes > in_n_bytes)
        {
            break;
        }

        apply_array(array,
                    in_snapshot_ptr + offset);

        offset += array.n_bytes;
    }

    bind_buffer_func_ptr(GL_ARRAY_BUFFER,
                         static_cast<GLuint>(saved_array_buffer) );

    if (client_active_func_ptr != nullptr)
    {
        client_active_func_ptr(static_cast<GLenum>(saved_client_active) );
    }

    result = true;

end:
    return result;
}

void Replay::StereoReplayEngineDLL::GlClientArrayReplayer::apply_array(const SnapshotArray& in_array,
                                                                       const uint8_t*       in_data_ptr)
{
    /* ClientArrayKind (see OpenGL/types_enums.h). */
    enum class ArrayKind : uint32_t
    {
        COLOR           = 0,
        EDGE_FLAG       = 1,
        FOG_COORD       = 2,
        GENERIC         = 3,
        INDEX           = 4,
        NORMAL          = 5,
        SECONDARY_COLOR = 6,
        TEX_COORD       = 7,
        VERTEX          = 8
    };

    /* The data starts at the first element which was copied, so the pointer is the one of the (missing) element 0. */
    const uintptr_t base    = reinterpret_cast<uintptr_t>  (in_data_ptr) - static_cast<uintptr_t>(in_array.first_element) * get_element_stride(in_array);
    const void*     pointer = reinterpret_cast<const void*>(base);

    switch (static_cast<ArrayKind>(in_array.kind) )
    {
        case ArrayKind::COLOR:
        {
            point_with_size("glColorPointer",
                            in_array,
                            pointer);

            break;
        }

        case ArrayKind::EDGE_FLAG:
        {
            point_edge_flag(in_array,
                            pointer);

            break;
        }

        case ArrayKind::FOG_COORD:
        {
            point_without_size("glFogCoordPointer",
                               in_array,
                               pointer);

            break;
        }

        case ArrayKind::GENERIC:
        {
            point_generic(in_array,
                          pointer);

            break;
        }

        case ArrayKind::INDEX:
        {
            point_without_size("glIndexPointer",
                               in_array,
                               pointer);

            break;
        }

        case ArrayKind::NORMAL:
        {
            point_without_size("glNormalPointer",
                               in_array,
                               pointer);

            break;
        }

        case ArrayKind::SECONDARY_COLOR:
        {
            point_with_size("glSecondaryColorPointer",
                            in_array,
                            pointer);

            break;
        }

        case ArrayKind::TEX_COORD:
        {
            point_tex_coord(in_array,
                            pointer);

            break;
        }

        case ArrayKind::VERTEX:
        {
            point_with_size("glVertexPointer",
                            in_array,
                            pointer);

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_array.kind);

            break;
        }
    }
}

uint32_t Replay::StereoReplayEngineDLL::GlClientArrayReplayer::get_element_stride(const SnapshotArray& in_array)
{
    uint32_t result = in_array.stride;

    if (result              == 0 &&
        in_array.n_elements != 0)
    {
        result = in_array.n_bytes / in_array.n_elements;
    }

    return result;
}

void Replay::StereoReplayEngineDLL::GlClientArrayReplayer::point_edge_flag(const SnapshotArray& in_array,
                                                                           const void*          in_pointer)
{
    typedef void (APIENTRY* PFNEDGEFLAGPOINTERPROC)(GLsizei, const void*);

    const auto pointer_func_ptr = reinterpret_cast<PFNEDGEFLAGPOINTERPROC>(m_resolve_func("glEdgeFlagPointer") );

    if (pointer_func_ptr != nullptr)
    {
        pointer_func_ptr(static_cast<GLsizei>(in_array.stride),
                         in_pointer);
    }
}

void Replay::StereoReplayEngineDLL::GlClientArrayReplayer::point_generic(const SnapshotArray& in_array,
                                                                         const void*          in_pointer)
{
    /* ClientArrayManager::SNAPSHOT_FLAG_*. */
    constexpr uint32_t FLAG_IS_INTEGER    = 1 << 0;
    constexpr uint32_t FLAG_IS_LONG       = 1 << 1;
    constexpr uint32_t FLAG_IS_NORMALIZED = 1 << 2;

    typedef void (APIENTRY* PFNVERTEXATTRIBDIVISORPROC) (GLuint, GLuint);
    typedef void (APIENTRY* PFNVERTEXATTRIBIPOINTERPROC)(GLuint, GLint, GLenum, GLsizei, const void*);
    typedef void (APIENTRY* PFNVERTEXATTRIBPOINTERPROC) (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

    const GLuint live_index = m_context_ptr->map_attrib_location(in_array.index);

    if ((in_array.flags & FLAG_IS_INTEGER) != 0)
    {
        const auto pointer_func_ptr = reinterpret_cast<PFNVERTEXATTRIBIPOINTERPROC>(m_resolve_func("glVertexAttribIPointer") );

        if (pointer_func_ptr != nullptr)
        {
            pointer_func_ptr(live_index,
                             in_array.size,
                             in_array.type,
                             static_cast<GLsizei>(in_array.stride),
                             in_pointer);
        }
    }
    else
    if ((in_array.flags & FLAG_IS_LONG) != 0)
    {
        const auto pointer_func_ptr = reinterpret_cast<PFNVERTEXATTRIBIPOINTERPROC>(m_resolve_func("glVertexAttribLPointer") );

        if (pointer_func_ptr != nullptr)
        {
            pointer_func_ptr(live_index,
                             in_array.size,
                             in_array.type,
                             static_cast<GLsizei>(in_array.stride),
                             in_pointer);
        }
    }
    else
    {
        const GLboolean normalized       = ((in_array.flags & FLAG_IS_NORMALIZED) != 0) ? GL_TRUE
                                                                                        : GL_FALSE;
        const auto      pointer_func_ptr = reinterpret_cast<PFNVERTEXATTRIBPOINTERPROC>(m_resolve_func("glVertexAttribPointer") );

        if (pointer_func_ptr != nullptr)
        {
            pointer_func_ptr(live_index,
                             in_array.size,
                             in_array.type,
                             normalized,
                             static_cast<GLsizei>(in_array.stride),
                             in_pointer);
        }
    }

    if (in_array.divisor != 0)
    {
        const auto divisor_func_ptr = reinterpret_cast<PFNVERTEXATTRIBDIVISORPROC>(m_resolve_func("glVertexAttribDivisor") );

        if (divisor_func_ptr != nullptr)
        {
            divisor_func_ptr(live_index,
                             in_array.divisor);
        }
    }
}

void Replay::StereoReplayEngineDLL::GlClientArrayReplayer::point_tex_coord(const SnapshotArray& in_array,
                                                                           const void*          in_pointer)
{
    typedef void (APIENTRY* PFNCLIENTACTIVETEXTUREPROC)(GLenum);
    typedef void (APIENTRY* PFNPOINTERWITHSIZEPROC)    (GLint, GLenum, GLsizei, const void*);

    const auto client_active_func_ptr = reinterpret_cast<PFNCLIENTACTIVETEXTUREPROC>(m_resolve_func("glClientActiveTexture") );
    const auto pointer_func_ptr       = reinterpret_cast<PFNPOINTERWITHSIZEPROC>(m_resolve_func("glTexCoordPointer") );

    /* The texture unit that the app pointed the array at. */
    if (client_active_func_ptr != nullptr &&
        pointer_func_ptr       != nullptr)
    {
        client_active_func_ptr(GL_TEXTURE0 + in_array.index);

        pointer_func_ptr(in_array.size,
                         in_array.type,
                         static_cast<GLsizei>(in_array.stride),
                         in_pointer);
    }
}

void Replay::StereoReplayEngineDLL::GlClientArrayReplayer::point_with_size(const char*          in_function_name_ptr,
                                                                           const SnapshotArray& in_array,
                                                                           const void*          in_pointer)
{
    typedef void (APIENTRY* PFNPOINTERWITHSIZEPROC)(GLint, GLenum, GLsizei, const void*);

    const auto pointer_func_ptr = reinterpret_cast<PFNPOINTERWITHSIZEPROC>(m_resolve_func(in_function_name_ptr) );

    if (pointer_func_ptr != nullptr)
    {
        pointer_func_ptr(in_array.size,
                         in_array.type,
                         static_cast<GLsizei>(in_array.stride),
                         in_pointer);
    }
}

void Replay::StereoReplayEngineDLL::GlClientArrayReplayer::point_without_size(const char*          in_function_name_ptr,
                                                                              const SnapshotArray& in_array,
                                                                              const void*          in_pointer)
{
    typedef void (APIENTRY* PFNPOINTERWITHOUTSIZEPROC)(GLenum, GLsizei, const void*);

    const auto pointer_func_ptr = reinterpret_cast<PFNPOINTERWITHOUTSIZEPROC>(m_resolve_func(in_function_name_ptr) );

    if (pointer_func_ptr != nullptr)
    {
        pointer_func_ptr(in_array.type,
                         static_cast<GLsizei>(in_array.stride),
                         in_pointer);
    }
}

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_extension_functions.h"
#include "interception/hooks/WGL/globals.h"
#include "own_call_scope.h"
#include <string>

template<typename FuncType>
bool Stereo::ConverterDLL::GLExtensionFunctions::find_function(const char* in_name_ptr,
                                                               FuncType*   out_func_ptr_ptr)
{
    const std::string extension_name = std::string(in_name_ptr) + "EXT";
    /* The original wglGetProcAddress, called through the cached pointer. The hooks that wglGetProcAddress returns for extension functions do not
     * have the driver's functions yet when the application makes its context current. */
    const auto        get_proc_address_func_ptr = reinterpret_cast<Interception::Hooks::WGL::PFNWGLGETPROCADDRESSPROC>(Interception::Hooks::WGL::g_cached_get_proc_address_func_ptr);
    PROC              proc_ptr       = get_proc_address_func_ptr(in_name_ptr);

    const auto is_valid_func = [](const PROC& in_proc_ptr)
    {
        const uintptr_t value = reinterpret_cast<uintptr_t>(in_proc_ptr);

        return (value > 3 && value != static_cast<uintptr_t>(-1) );
    };

    if (is_valid_func(proc_ptr) == false)
    {
        proc_ptr = get_proc_address_func_ptr(extension_name.c_str() );
    }

    const bool result = is_valid_func(proc_ptr);

    *out_func_ptr_ptr = (result) ? reinterpret_cast<FuncType>(proc_ptr)
                                 : nullptr;

    return result;
}

bool Stereo::ConverterDLL::GLExtensionFunctions::load()
{
    /* The functions are looked up through the library that the hooks watch. OwnCallScope marks these calls as the converter's own. */
    const OwnCallScope own_call_scope;
    bool               result = true;

    result &= find_function("glBindFramebuffer",       &bind_framebuffer_func_ptr);
    result &= find_function("glBindRenderbuffer",      &bind_renderbuffer_func_ptr);
    result &= find_function("glCheckFramebufferStatus",&check_framebuffer_status_func_ptr);
    result &= find_function("glDeleteFramebuffers",    &delete_framebuffers_func_ptr);
    result &= find_function("glDeleteRenderbuffers",   &delete_renderbuffers_func_ptr);
    result &= find_function("glFramebufferRenderbuffer",&framebuffer_renderbuffer_func_ptr);
    result &= find_function("glGenFramebuffers",       &gen_framebuffers_func_ptr);
    result &= find_function("glGenRenderbuffers",      &gen_renderbuffers_func_ptr);
    result &= find_function("glRenderbufferStorage",   &renderbuffer_storage_func_ptr);

    find_function("glActiveTexture",   &active_texture_func_ptr);
    find_function("glBlitFramebuffer", &blit_framebuffer_func_ptr);

    /* The buffer functions are part of OpenGL 1.5. Either all of them are used or none. */
    if (find_function("glBindBuffer",    &bind_buffer_func_ptr)    == false ||
        find_function("glBufferData",    &buffer_data_func_ptr)    == false ||
        find_function("glDeleteBuffers", &delete_buffers_func_ptr) == false ||
        find_function("glGenBuffers",    &gen_buffers_func_ptr)    == false ||
        find_function("glMapBuffer",     &map_buffer_func_ptr)     == false ||
        find_function("glUnmapBuffer",   &unmap_buffer_func_ptr)   == false)
    {
        bind_buffer_func_ptr    = nullptr;
        buffer_data_func_ptr    = nullptr;
        delete_buffers_func_ptr = nullptr;
        gen_buffers_func_ptr    = nullptr;
        map_buffer_func_ptr     = nullptr;
        unmap_buffer_func_ptr   = nullptr;
    }

    return result;
}

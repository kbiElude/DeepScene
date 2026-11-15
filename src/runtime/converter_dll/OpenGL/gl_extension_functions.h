/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_EXTENSION_FUNCTIONS_H
#define STEREO_CONVERTERDLL_GL_EXTENSION_FUNCTIONS_H

#include <Windows.h>
#include <GL/gl.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The OpenGL functions that opengl32.dll does not export (framebuffers, buffers, the active texture unit). They are looked up through the current context. */
        struct GLExtensionFunctions
        {
            typedef void (APIENTRY* ActiveTextureFunc)        (GLenum in_texture);
            typedef void (APIENTRY* BindBufferFunc)           (GLenum in_target, GLuint in_buffer);
            typedef void (APIENTRY* BindFramebufferFunc)      (GLenum in_target, GLuint in_framebuffer);
            typedef void (APIENTRY* BindRenderbufferFunc)     (GLenum in_target, GLuint in_renderbuffer);
            typedef void (APIENTRY* BlitFramebufferFunc)      (GLint in_source_x0, GLint in_source_y0, GLint in_source_x1, GLint in_source_y1,
                                                               GLint in_destination_x0, GLint in_destination_y0, GLint in_destination_x1, GLint in_destination_y1,
                                                               GLbitfield in_mask, GLenum in_filter);
            typedef void (APIENTRY* BufferDataFunc)           (GLenum in_target, ptrdiff_t in_n_bytes, const void* in_data_ptr, GLenum in_usage);
            typedef GLenum (APIENTRY* CheckFramebufferStatusFunc)(GLenum in_target);
            typedef void (APIENTRY* DeleteBuffersFunc)        (GLsizei in_n, const GLuint* in_buffer_ptr);
            typedef void (APIENTRY* DeleteFramebuffersFunc)   (GLsizei in_n, const GLuint* in_framebuffer_ptr);
            typedef void (APIENTRY* DeleteRenderbuffersFunc)  (GLsizei in_n, const GLuint* in_renderbuffer_ptr);
            typedef void (APIENTRY* FramebufferRenderbufferFunc)(GLenum in_target, GLenum in_attachment, GLenum in_renderbuffer_target, GLuint in_renderbuffer);
            typedef void (APIENTRY* GenBuffersFunc)           (GLsizei in_n, GLuint* out_buffer_ptr);
            typedef void (APIENTRY* GenFramebuffersFunc)      (GLsizei in_n, GLuint* out_framebuffer_ptr);
            typedef void (APIENTRY* GenRenderbuffersFunc)     (GLsizei in_n, GLuint* out_renderbuffer_ptr);
            typedef void* (APIENTRY* MapBufferFunc)           (GLenum in_target, GLenum in_access);
            typedef void (APIENTRY* RenderbufferStorageFunc)  (GLenum in_target, GLenum in_internal_format, GLsizei in_width, GLsizei in_height);
            typedef GLboolean (APIENTRY* UnmapBufferFunc)     (GLenum in_target);

            ActiveTextureFunc           active_texture_func_ptr            = nullptr; /* optional: needed only by a program that uses more than one texture unit */
            BindBufferFunc              bind_buffer_func_ptr               = nullptr; /* optional: without buffers, the images of the eyes are read immediately */
            BindFramebufferFunc         bind_framebuffer_func_ptr          = nullptr;
            BindRenderbufferFunc        bind_renderbuffer_func_ptr         = nullptr;
            BlitFramebufferFunc         blit_framebuffer_func_ptr          = nullptr; /* optional: shows the left eye in the application's window when no other program shows the images */
            BufferDataFunc              buffer_data_func_ptr               = nullptr;
            CheckFramebufferStatusFunc  check_framebuffer_status_func_ptr  = nullptr;
            DeleteBuffersFunc           delete_buffers_func_ptr            = nullptr;
            DeleteFramebuffersFunc      delete_framebuffers_func_ptr       = nullptr;
            DeleteRenderbuffersFunc     delete_renderbuffers_func_ptr      = nullptr;
            FramebufferRenderbufferFunc framebuffer_renderbuffer_func_ptr  = nullptr;
            GenBuffersFunc              gen_buffers_func_ptr               = nullptr;
            GenFramebuffersFunc         gen_framebuffers_func_ptr          = nullptr;
            GenRenderbuffersFunc        gen_renderbuffers_func_ptr         = nullptr;
            MapBufferFunc               map_buffer_func_ptr                = nullptr;
            RenderbufferStorageFunc     renderbuffer_storage_func_ptr      = nullptr;
            UnmapBufferFunc             unmap_buffer_func_ptr              = nullptr;

            /* Finds the functions, using the extension (EXT) versions when the context lacks the core ones. Returns false if a required function is missing. */
            bool load();

        private:
            /* wglGetProcAddress reports a missing function with a few small values (1, 2, 3 and -1) as well as with zero. */
            template<typename FuncType>
            static bool find_function(const char* in_name_ptr,
                                      FuncType*   out_func_ptr_ptr);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_EXTENSION_FUNCTIONS_H */

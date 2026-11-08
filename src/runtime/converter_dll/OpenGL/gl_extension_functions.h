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
        /* The functions of OpenGL that opengl32.dll does not export (framebuffers, the active texture unit), found through the context that is current. */
        struct GlExtensionFunctions
        {
            typedef void (APIENTRY* ActiveTextureFunc)        (GLenum in_texture);
            typedef void (APIENTRY* BindFramebufferFunc)      (GLenum in_target, GLuint in_framebuffer);
            typedef void (APIENTRY* BindRenderbufferFunc)     (GLenum in_target, GLuint in_renderbuffer);
            typedef void (APIENTRY* BlitFramebufferFunc)      (GLint in_source_x0, GLint in_source_y0, GLint in_source_x1, GLint in_source_y1,
                                                               GLint in_destination_x0, GLint in_destination_y0, GLint in_destination_x1, GLint in_destination_y1,
                                                               GLbitfield in_mask, GLenum in_filter);
            typedef GLenum (APIENTRY* CheckFramebufferStatusFunc)(GLenum in_target);
            typedef void (APIENTRY* DeleteFramebuffersFunc)   (GLsizei in_n, const GLuint* in_framebuffer_ptr);
            typedef void (APIENTRY* DeleteRenderbuffersFunc)  (GLsizei in_n, const GLuint* in_renderbuffer_ptr);
            typedef void (APIENTRY* FramebufferRenderbufferFunc)(GLenum in_target, GLenum in_attachment, GLenum in_renderbuffer_target, GLuint in_renderbuffer);
            typedef void (APIENTRY* GenFramebuffersFunc)      (GLsizei in_n, GLuint* out_framebuffer_ptr);
            typedef void (APIENTRY* GenRenderbuffersFunc)     (GLsizei in_n, GLuint* out_renderbuffer_ptr);
            typedef void (APIENTRY* RenderbufferStorageFunc)  (GLenum in_target, GLenum in_internal_format, GLsizei in_width, GLsizei in_height);

            ActiveTextureFunc           active_texture_func_ptr            = nullptr; /* optional: only a program that uses more than one unit needs it */
            BindFramebufferFunc         bind_framebuffer_func_ptr          = nullptr;
            BindRenderbufferFunc        bind_renderbuffer_func_ptr         = nullptr;
            BlitFramebufferFunc         blit_framebuffer_func_ptr          = nullptr; /* optional: shows the left eye in the window of a program that shows nothing elsewhere */
            CheckFramebufferStatusFunc  check_framebuffer_status_func_ptr  = nullptr;
            DeleteFramebuffersFunc      delete_framebuffers_func_ptr       = nullptr;
            DeleteRenderbuffersFunc     delete_renderbuffers_func_ptr      = nullptr;
            FramebufferRenderbufferFunc framebuffer_renderbuffer_func_ptr  = nullptr;
            GenFramebuffersFunc         gen_framebuffers_func_ptr          = nullptr;
            GenRenderbuffersFunc        gen_renderbuffers_func_ptr         = nullptr;
            RenderbufferStorageFunc     renderbuffer_storage_func_ptr      = nullptr;

            /* Finds the functions (the ones of the extension if the context has not got the ones of the version); false if the ones that are needed are missing. */
            bool load();
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_EXTENSION_FUNCTIONS_H */

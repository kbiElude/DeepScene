/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_DRIVER_FUNCTIONS_H
#define STEREO_CONVERTERDLL_GL_DRIVER_FUNCTIONS_H

#include "interception/hooks/OpenGL/globals.h"

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The OpenGL 1.1 functions that the converter calls, taken directly from the driver. They are not the hooks. A hook also runs the
         * callbacks and records the data of the call (a pixel read copies its pixels) for a program that watches the application. The converter's
         * own calls need none of that.
         */
        struct GLDriverFunctions
        {
            PFNGLBINDTEXTUREPROC             bind_texture_func_ptr               = nullptr;
            PFNGLCALLLISTPROC                call_list_func_ptr                  = nullptr;
            PFNGLCLEARPROC                   clear_func_ptr                      = nullptr;
            PFNGLCOPYTEXIMAGE2DPROC          copy_tex_image_2d_func_ptr          = nullptr;
            PFNGLCOPYTEXSUBIMAGE2DPROC       copy_tex_sub_image_2d_func_ptr      = nullptr;
            PFNGLDELETETEXTURESPROC          delete_textures_func_ptr            = nullptr;
            PFNGLDISABLEPROC                 disable_func_ptr                    = nullptr;
            PFNGLDRAWARRAYSPROC              draw_arrays_func_ptr                = nullptr;
            PFNGLDRAWELEMENTSPROC            draw_elements_func_ptr              = nullptr;
            PFNGLENABLEPROC                  enable_func_ptr                     = nullptr;
            PFNGLENDLISTPROC                 end_list_func_ptr                   = nullptr;
            PFNGLGENLISTSPROC                gen_lists_func_ptr                  = nullptr;
            PFNGLGENTEXTURESPROC             gen_textures_func_ptr               = nullptr;
            PFNGLGETFLOATVPROC               get_floatv_func_ptr                 = nullptr;
            PFNGLGETINTEGERVPROC             get_integerv_func_ptr               = nullptr;
            PFNGLGETTEXLEVELPARAMETERIVPROC  get_tex_level_parameteriv_func_ptr  = nullptr;
            PFNGLGETTEXPARAMETERIVPROC       get_tex_parameteriv_func_ptr        = nullptr;
            PFNGLISENABLEDPROC               is_enabled_func_ptr                 = nullptr;
            PFNGLLINEWIDTHPROC               line_width_func_ptr                 = nullptr;
            PFNGLLOADMATRIXFPROC             load_matrixf_func_ptr               = nullptr;
            PFNGLMATRIXMODEPROC              matrix_mode_func_ptr                = nullptr;
            PFNGLMULTMATRIXFPROC             mult_matrixf_func_ptr               = nullptr;
            PFNGLNEWLISTPROC                 new_list_func_ptr                   = nullptr;
            PFNGLPIXELSTOREIPROC             pixel_storei_func_ptr               = nullptr;
            PFNGLPOINTSIZEPROC               point_size_func_ptr                 = nullptr;
            PFNGLREADPIXELSPROC              read_pixels_func_ptr                = nullptr;
            PFNGLSCISSORPROC                 scissor_func_ptr                    = nullptr;
            PFNGLTEXIMAGE2DPROC              tex_image_2d_func_ptr               = nullptr;
            PFNGLTEXPARAMETERFPROC           tex_parameterf_func_ptr             = nullptr;
            PFNGLTEXPARAMETERIPROC           tex_parameteri_func_ptr             = nullptr;
            PFNGLVIEWPORTPROC                viewport_func_ptr                   = nullptr;

            /* The functions, found once. */
            static const GLDriverFunctions& get();
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_DRIVER_FUNCTIONS_H */

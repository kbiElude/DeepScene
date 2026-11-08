/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_driver_functions.h"

const Stereo::ConverterDLL::GlDriverFunctions& Stereo::ConverterDLL::GlDriverFunctions::get()
{
    namespace Hooks = Interception::Hooks::OpenGL;

    static const GlDriverFunctions result = []()
    {
        GlDriverFunctions functions;

        functions.bind_texture_func_ptr              = reinterpret_cast<PFNGLBINDTEXTUREPROC>            (Hooks::g_cached_gl_bind_texture);
        functions.call_list_func_ptr                 = reinterpret_cast<PFNGLCALLLISTPROC>               (Hooks::g_cached_gl_call_list);
        functions.clear_func_ptr                     = reinterpret_cast<PFNGLCLEARPROC>                  (Hooks::g_cached_gl_clear);
        functions.copy_tex_image_2d_func_ptr         = reinterpret_cast<PFNGLCOPYTEXIMAGE2DPROC>         (Hooks::g_cached_gl_copy_tex_image_2d);
        functions.copy_tex_sub_image_2d_func_ptr     = reinterpret_cast<PFNGLCOPYTEXSUBIMAGE2DPROC>      (Hooks::g_cached_gl_copy_tex_sub_image_2d);
        functions.delete_textures_func_ptr           = reinterpret_cast<PFNGLDELETETEXTURESPROC>         (Hooks::g_cached_gl_delete_textures);
        functions.draw_arrays_func_ptr               = reinterpret_cast<PFNGLDRAWARRAYSPROC>             (Hooks::g_cached_gl_draw_arrays);
        functions.draw_elements_func_ptr             = reinterpret_cast<PFNGLDRAWELEMENTSPROC>           (Hooks::g_cached_gl_draw_elements);
        functions.end_list_func_ptr                  = reinterpret_cast<PFNGLENDLISTPROC>                (Hooks::g_cached_gl_end_list);
        functions.gen_lists_func_ptr                 = reinterpret_cast<PFNGLGENLISTSPROC>               (Hooks::g_cached_gl_gen_lists);
        functions.gen_textures_func_ptr              = reinterpret_cast<PFNGLGENTEXTURESPROC>            (Hooks::g_cached_gl_gen_textures);
        functions.get_floatv_func_ptr                = reinterpret_cast<PFNGLGETFLOATVPROC>              (Hooks::g_cached_gl_get_floatv);
        functions.get_integerv_func_ptr              = reinterpret_cast<PFNGLGETINTEGERVPROC>            (Hooks::g_cached_gl_get_integerv);
        functions.get_tex_level_parameteriv_func_ptr = reinterpret_cast<PFNGLGETTEXLEVELPARAMETERIVPROC>(Hooks::g_cached_gl_get_tex_level_parameteriv);
        functions.get_tex_parameteriv_func_ptr       = reinterpret_cast<PFNGLGETTEXPARAMETERIVPROC>      (Hooks::g_cached_gl_get_tex_parameteriv);
        functions.load_matrixf_func_ptr              = reinterpret_cast<PFNGLLOADMATRIXFPROC>            (Hooks::g_cached_gl_load_matrix_f);
        functions.matrix_mode_func_ptr               = reinterpret_cast<PFNGLMATRIXMODEPROC>             (Hooks::g_cached_gl_matrix_mode);
        functions.mult_matrixf_func_ptr              = reinterpret_cast<PFNGLMULTMATRIXFPROC>            (Hooks::g_cached_gl_mult_matrix_f);
        functions.new_list_func_ptr                  = reinterpret_cast<PFNGLNEWLISTPROC>                (Hooks::g_cached_gl_new_list);
        functions.pixel_storei_func_ptr              = reinterpret_cast<PFNGLPIXELSTOREIPROC>            (Hooks::g_cached_gl_pixel_storei);
        functions.read_pixels_func_ptr               = reinterpret_cast<PFNGLREADPIXELSPROC>             (Hooks::g_cached_gl_read_pixels);
        functions.tex_image_2d_func_ptr              = reinterpret_cast<PFNGLTEXIMAGE2DPROC>             (Hooks::g_cached_gl_tex_image_2D);
        functions.tex_parameterf_func_ptr            = reinterpret_cast<PFNGLTEXPARAMETERFPROC>          (Hooks::g_cached_gl_tex_parameterf);
        functions.tex_parameteri_func_ptr            = reinterpret_cast<PFNGLTEXPARAMETERIPROC>          (Hooks::g_cached_gl_tex_parameteri);

        return functions;
    }();

    return result;
}

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
        functions.disable_func_ptr                   = reinterpret_cast<PFNGLDISABLEPROC>                (Hooks::g_cached_gl_disable);
        functions.draw_arrays_func_ptr               = reinterpret_cast<PFNGLDRAWARRAYSPROC>             (Hooks::g_cached_gl_draw_arrays);
        functions.draw_elements_func_ptr             = reinterpret_cast<PFNGLDRAWELEMENTSPROC>           (Hooks::g_cached_gl_draw_elements);
        functions.enable_func_ptr                    = reinterpret_cast<PFNGLENABLEPROC>                 (Hooks::g_cached_gl_enable);
        functions.end_list_func_ptr                  = reinterpret_cast<PFNGLENDLISTPROC>                (Hooks::g_cached_gl_end_list);
        functions.gen_lists_func_ptr                 = reinterpret_cast<PFNGLGENLISTSPROC>               (Hooks::g_cached_gl_gen_lists);
        functions.gen_textures_func_ptr              = reinterpret_cast<PFNGLGENTEXTURESPROC>            (Hooks::g_cached_gl_gen_textures);
        functions.get_floatv_func_ptr                = reinterpret_cast<PFNGLGETFLOATVPROC>              (Hooks::g_cached_gl_get_floatv);
        functions.get_integerv_func_ptr              = reinterpret_cast<PFNGLGETINTEGERVPROC>            (Hooks::g_cached_gl_get_integerv);
        functions.get_tex_level_parameteriv_func_ptr = reinterpret_cast<PFNGLGETTEXLEVELPARAMETERIVPROC>(Hooks::g_cached_gl_get_tex_level_parameteriv);
        functions.get_tex_parameteriv_func_ptr       = reinterpret_cast<PFNGLGETTEXPARAMETERIVPROC>      (Hooks::g_cached_gl_get_tex_parameteriv);
        functions.is_enabled_func_ptr                = reinterpret_cast<PFNGLISENABLEDPROC>              (Hooks::g_cached_gl_is_enabled);
        functions.line_width_func_ptr                = reinterpret_cast<PFNGLLINEWIDTHPROC>              (Hooks::g_cached_gl_line_width);
        functions.load_matrixf_func_ptr              = reinterpret_cast<PFNGLLOADMATRIXFPROC>            (Hooks::g_cached_gl_load_matrix_f);
        functions.matrix_mode_func_ptr               = reinterpret_cast<PFNGLMATRIXMODEPROC>             (Hooks::g_cached_gl_matrix_mode);
        functions.mult_matrixf_func_ptr              = reinterpret_cast<PFNGLMULTMATRIXFPROC>            (Hooks::g_cached_gl_mult_matrix_f);
        functions.new_list_func_ptr                  = reinterpret_cast<PFNGLNEWLISTPROC>                (Hooks::g_cached_gl_new_list);
        functions.pixel_storei_func_ptr              = reinterpret_cast<PFNGLPIXELSTOREIPROC>            (Hooks::g_cached_gl_pixel_storei);
        functions.point_size_func_ptr                = reinterpret_cast<PFNGLPOINTSIZEPROC>              (Hooks::g_cached_gl_point_size);
        functions.read_pixels_func_ptr               = reinterpret_cast<PFNGLREADPIXELSPROC>             (Hooks::g_cached_gl_read_pixels);
        functions.scissor_func_ptr                   = reinterpret_cast<PFNGLSCISSORPROC>                (Hooks::g_cached_gl_scissor);
        functions.tex_image_2d_func_ptr              = reinterpret_cast<PFNGLTEXIMAGE2DPROC>             (Hooks::g_cached_gl_tex_image_2D);
        functions.tex_parameterf_func_ptr            = reinterpret_cast<PFNGLTEXPARAMETERFPROC>          (Hooks::g_cached_gl_tex_parameterf);
        functions.tex_parameteri_func_ptr            = reinterpret_cast<PFNGLTEXPARAMETERIPROC>          (Hooks::g_cached_gl_tex_parameteri);
        functions.viewport_func_ptr                  = reinterpret_cast<PFNGLVIEWPORTPROC>               (Hooks::g_cached_gl_viewport);

        return functions;
    }();

    return result;
}

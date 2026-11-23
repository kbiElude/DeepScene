/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/window_system_state_effect_describer.h"
#include "capture/api_dump_loader/api_info.h"
#include "common/macros.h"

Analysis::APIDumpIndex::WindowSystemStateEffectDescriber::WindowSystemStateEffectDescriber(const Capture::APIDumpLoader::APIGroup& in_group)
    :m_group(in_group)
{
    AI_ASSERT_ALWAYS(in_group == Capture::APIDumpLoader::APIGroup::GDI32  ||
                     in_group == Capture::APIDumpLoader::APIGroup::USER32 ||
                     in_group == Capture::APIDumpLoader::APIGroup::WGL,
                     "A window system describer is made for GDI32, USER32 or WGL only.");
}

Analysis::APIDumpIndex::StateEffect Analysis::APIDumpIndex::WindowSystemStateEffectDescriber::describe(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    StateEffect result;

    AI_ASSERT_ALWAYS(Capture::APIDumpLoader::get_api_group(in_call.func) == m_group,
                     "A call was given to the describer of another API.");

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GDI32_CHOOSEPIXELFORMAT:
        case APIInterceptor::APIFUNCTION_GDI32_DESCRIBEPIXELFORMAT:
        case APIInterceptor::APIFUNCTION_GDI32_GETPIXELFORMAT:
        case APIInterceptor::APIFUNCTION_GDI32_SETPIXELFORMAT:
        case APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS:
        case APIInterceptor::APIFUNCTION_USER32_ADJUSTWINDOWRECTEX:
        case APIInterceptor::APIFUNCTION_USER32_CREATEDCA:
        case APIInterceptor::APIFUNCTION_USER32_CREATEDCW:
        case APIInterceptor::APIFUNCTION_USER32_CREATEWINDOWEXA:
        case APIInterceptor::APIFUNCTION_USER32_CREATEWINDOWEXW:
        case APIInterceptor::APIFUNCTION_USER32_GETDC:
        case APIInterceptor::APIFUNCTION_USER32_MOVEWINDOW:
        case APIInterceptor::APIFUNCTION_USER32_RELEASEDC:
        case APIInterceptor::APIFUNCTION_USER32_SETWINDOWPOS:
        case APIInterceptor::APIFUNCTION_WGL_WGLCOPYCONTEXT:
        case APIInterceptor::APIFUNCTION_WGL_WGLCREATECONTEXT:
        case APIInterceptor::APIFUNCTION_WGL_WGLCREATECONTEXTATTRIBSARB:
        case APIInterceptor::APIFUNCTION_WGL_WGLCREATELAYERCONTEXT:
        case APIInterceptor::APIFUNCTION_WGL_WGLDELETECONTEXT:
        case APIInterceptor::APIFUNCTION_WGL_WGLGETCURRENTCONTEXT:
        case APIInterceptor::APIFUNCTION_WGL_WGLGETCURRENTDC:
        case APIInterceptor::APIFUNCTION_WGL_WGLGETEXTENSIONSSTRINGARB:
        case APIInterceptor::APIFUNCTION_WGL_WGLGETSWAPINTERVALEXT:
        case APIInterceptor::APIFUNCTION_WGL_WGLMAKECURRENT:
        case APIInterceptor::APIFUNCTION_WGL_WGLSHARELISTS:
        case APIInterceptor::APIFUNCTION_WGL_WGLSWAPINTERVALEXT:
        case APIInterceptor::APIFUNCTION_WGL_WGLSWAPLAYERBUFFERS:
        {
            result.kind = StateEffectKind::BARRIER;

            break;
        }

        default:
        {
            /* A function that was added to the interceptor: say here what it does to state. */
            AI_FAIL_UNSUPPORTED(in_call.func);
        }
    }

    return result;
}

Capture::APIDumpLoader::APIGroup Analysis::APIDumpIndex::WindowSystemStateEffectDescriber::get_group() const
{
    return m_group;
}

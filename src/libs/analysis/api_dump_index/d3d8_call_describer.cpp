/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/d3d8_call_describer.h"

Analysis::APIDumpIndex::CallClass Analysis::APIDumpIndex::D3D8CallDescriber::describe(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    return get_function_class(in_call.func);
}

Analysis::APIDumpIndex::CallClass Analysis::APIDumpIndex::D3D8CallDescriber::get_function_class(const APIInterceptor::APIFunction& in_function)
{
    CallClass result = CallClass::STATE;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_COPYRECTS:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_LOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_UNLOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_LOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_UNLOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_LOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_UNLOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_LOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_UNLOCK:
        {
            result = CallClass::CONTENT;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

Capture::APIDumpLoader::APIGroup Analysis::APIDumpIndex::D3D8CallDescriber::get_group() const
{
    return Capture::APIDumpLoader::APIGroup::D3D8;
}

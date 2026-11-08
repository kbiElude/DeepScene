/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for D3D7FunctionPolicy and D3D8FunctionPolicy: which functions the front ends may leave out, which end the process, and which they follow. The set of the functions that
 * are in neither group is checked against the list of those that the front end follows: a function that is added to the API, or taken out of a table, has to be
 * decided on, and cannot be passed over.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/D3D7/d3d7_function_policy.h"
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_function_policy.h"
#include "capture/api_dump_names/function_names.h"
#include "common/types.h"
#include <set>
#include <string>
#include <string.h>

namespace
{
    typedef Analysis::APIDumpAnalysis::Frontends::D3D7FunctionPolicy D3D7FunctionPolicy;
    typedef Analysis::APIDumpAnalysis::Frontends::D3D8FunctionPolicy D3D8FunctionPolicy;

    /* Says each name that only one of the sets has (a set of many names is cut short when it is printed). */
    void expect_same_names(const std::set<std::string>& in_undecided_name_set,
                           const std::set<std::string>& in_followed_name_set)
    {
        for (const auto& name : in_undecided_name_set)
        {
            EXPECT_TRUE(in_followed_name_set.find(name) != in_followed_name_set.end() ) << name << " is neither left out nor does it end the process, and the front end does not follow it";
        }

        for (const auto& name : in_followed_name_set)
        {
            EXPECT_TRUE(in_undecided_name_set.find(name) != in_undecided_name_set.end() ) << name << " is said to be followed, and a table has it";
        }
    }

    /* The functions that the policy neither leaves out nor ends the process on, with the names that the printer gives them, and without the Release methods (followed by
     * every front end for every interface) and the queries (left out by their names, though the front end follows some of them).
     */
    template <typename LeftOutFunc,
              typename UnsupportedFunc>
    std::set<std::string> get_undecided_name_set(const APIInterceptor::APIFunction& in_first,
                                                 const APIInterceptor::APIFunction& in_last,
                                                 const LeftOutFunc&                 in_left_out_func,
                                                 const UnsupportedFunc&             in_unsupported_func)
    {
        std::set<std::string> result;

        for (uint32_t function = static_cast<uint32_t>(in_first);
                      function <= static_cast<uint32_t>(in_last);
                    ++function)
        {
            const auto  api_function = static_cast<APIInterceptor::APIFunction>(function);
            const char* name_ptr     = Capture::APIDumpNames::get_function_name(api_function);

            if (name_ptr != nullptr)
            {
                const std::string name(name_ptr);

                if (in_left_out_func    (api_function) == false &&
                    in_unsupported_func (api_function) == false &&
                    name.size() > strlen("::Release")           &&
                    name.compare(name.size() - strlen("::Release"),
                                 strlen("::Release"),
                                 "::Release") != 0)
                {
                    result.insert(name);
                }
            }
        }

        return result;
    }
}

TEST(D3D7FunctionPolicy, TheFunctionsThatAreNeitherLeftOutNorEndTheProcessAreTheOnesThatTheFrontEndFollows)
{
    const std::set<std::string> followed_name_set =
    {
        "IDirect3D7::CreateDevice",
        "IDirect3D7::CreateVertexBuffer",
        "IDirect3DDevice7::Clear",
        "IDirect3DDevice7::DrawIndexedPrimitive",
        "IDirect3DDevice7::DrawIndexedPrimitiveStrided",
        "IDirect3DDevice7::DrawIndexedPrimitiveVB",
        "IDirect3DDevice7::DrawPrimitive",
        "IDirect3DDevice7::DrawPrimitiveStrided",
        "IDirect3DDevice7::DrawPrimitiveVB",
        "IDirect3DDevice7::LightEnable",
        "IDirect3DDevice7::Load",
        "IDirect3DDevice7::SetLight",
        "IDirect3DDevice7::SetRenderState",
        "IDirect3DDevice7::SetRenderTarget",
        "IDirect3DDevice7::SetTexture",
        "IDirect3DDevice7::SetTextureStageState",
        "IDirect3DDevice7::SetTransform",
        "IDirect3DDevice7::SetViewport",
        "IDirect3DVertexBuffer7::Unlock",
        "IDirectDraw7::CreateSurface",
        "IDirectDraw7::DuplicateSurface",
        "IDirectDraw7::SetDisplayMode",
        "IDirectDrawSurface7::AddAttachedSurface",
        "IDirectDrawSurface7::Blt",
        "IDirectDrawSurface7::BltFast",
        "IDirectDrawSurface7::Flip",
        "IDirectDrawSurface7::Unlock"
    };

    const auto undecided_name_set = get_undecided_name_set(APIInterceptor::APIFUNCTION_D3D7_FIRST,
                                                           APIInterceptor::APIFUNCTION_D3D7_LAST,
                                                           &D3D7FunctionPolicy::is_left_out_by_design,
                                                           [](const APIInterceptor::APIFunction& in_function)
                                                           {
                                                               return D3D7FunctionPolicy::get_unsupported_reason_ptr(in_function) != nullptr;
                                                           });

    expect_same_names(undecided_name_set,
                      followed_name_set);
}

TEST(D3D7FunctionPolicy, WhatTheKasparovCaptureCallsIsFollowedOrLeftOut)
{
    const APIInterceptor::APIFunction left_out_array[] =
    {
        APIInterceptor::APIFUNCTION_D3D7_DIRECTDRAWCREATEEX,
        APIInterceptor::APIFUNCTION_D3D7_DIRECTDRAWENUMERATEEXA,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECT3D7_ENUMDEVICES,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECT3D7_ENUMZBUFFERFORMATS,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_BEGINSCENE,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_ENDSCENE,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_GETTRANSFORM,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_QUERYINTERFACE,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_SETCOOPERATIVELEVEL,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_ADDREF,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETSURFACEDESC,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_ISLOST,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_LOCK
    };

    for (const auto function : left_out_array)
    {
        EXPECT_TRUE   (D3D7FunctionPolicy::is_left_out_by_design(function) ) << Capture::APIDumpNames::get_function_name(function);
        EXPECT_EQ     (D3D7FunctionPolicy::get_unsupported_reason_ptr(function),
                       nullptr)                                               << Capture::APIDumpNames::get_function_name(function);
    }
}

TEST(D3D7FunctionPolicy, WhatChangesASurfaceOrTheScreenWithNoDataInTheWorkloadEndsTheProcess)
{
    const APIInterceptor::APIFunction unsupported_array[] =
    {
        APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_BEGINSTATEBLOCK,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_MULTIPLYTRANSFORM,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DVERTEXBUFFER7_PROCESSVERTICES,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATEPALETTE,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTBATCH,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETDC,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_SETCOLORKEY,
        APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_UPDATEOVERLAY
    };

    for (const auto function : unsupported_array)
    {
        EXPECT_NE(D3D7FunctionPolicy::get_unsupported_reason_ptr(function),
                  nullptr) << Capture::APIDumpNames::get_function_name(function);
    }
}

TEST(D3D8FunctionPolicy, TheFunctionsThatAreNeitherLeftOutNorEndTheProcessAreTheOnesThatTheFrontEndFollows)
{
    const std::set<std::string> followed_name_set =
    {
        "IDirect3D8::CreateDevice",
        "IDirect3DCubeTexture8::UnlockRect",
        "IDirect3DDevice8::Clear",
        "IDirect3DDevice8::CopyRects",
        "IDirect3DDevice8::CreateCubeTexture",
        "IDirect3DDevice8::CreateDepthStencilSurface",
        "IDirect3DDevice8::CreateImageSurface",
        "IDirect3DDevice8::CreateIndexBuffer",
        "IDirect3DDevice8::CreatePixelShader",
        "IDirect3DDevice8::CreateRenderTarget",
        "IDirect3DDevice8::CreateTexture",
        "IDirect3DDevice8::CreateVertexBuffer",
        "IDirect3DDevice8::CreateVertexShader",
        "IDirect3DDevice8::CreateVolumeTexture",
        "IDirect3DDevice8::DeletePixelShader",
        "IDirect3DDevice8::DeleteVertexShader",
        "IDirect3DDevice8::DrawIndexedPrimitive",
        "IDirect3DDevice8::DrawIndexedPrimitiveUP",
        "IDirect3DDevice8::DrawPrimitive",
        "IDirect3DDevice8::DrawPrimitiveUP",
        "IDirect3DDevice8::LightEnable",
        "IDirect3DDevice8::Present",
        "IDirect3DDevice8::Reset",
        "IDirect3DDevice8::SetIndices",
        "IDirect3DDevice8::SetLight",
        "IDirect3DDevice8::SetPixelShader",
        "IDirect3DDevice8::SetRenderState",
        "IDirect3DDevice8::SetRenderTarget",
        "IDirect3DDevice8::SetStreamSource",
        "IDirect3DDevice8::SetTexture",
        "IDirect3DDevice8::SetTextureStageState",
        "IDirect3DDevice8::SetTransform",
        "IDirect3DDevice8::SetVertexShader",
        "IDirect3DDevice8::SetViewport",
        "IDirect3DDevice8::UpdateTexture",
        "IDirect3DIndexBuffer8::Unlock",
        "IDirect3DSurface8::UnlockRect",
        "IDirect3DTexture8::UnlockRect",
        "IDirect3DVertexBuffer8::Unlock"
    };

    const auto undecided_name_set = get_undecided_name_set(APIInterceptor::APIFUNCTION_D3D8_FIRST,
                                                           APIInterceptor::APIFUNCTION_D3D8_LAST,
                                                           &D3D8FunctionPolicy::is_left_out_by_design,
                                                           [](const APIInterceptor::APIFunction& in_function)
                                                           {
                                                               return D3D8FunctionPolicy::get_unsupported_reason_ptr(in_function) != nullptr;
                                                           });

    expect_same_names(undecided_name_set,
                      followed_name_set);
}

TEST(D3D8FunctionPolicy, WhatChangesASurfaceAVolumeOrTheScreenWithNoDataInTheWorkloadEndsTheProcess)
{
    const APIInterceptor::APIFunction unsupported_array[] =
    {
        APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_BEGINSTATEBLOCK,
        APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEADDITIONALSWAPCHAIN,
        APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWRECTPATCH,
        APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETFRONTBUFFER,
        APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_MULTIPLYTRANSFORM,
        APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PROCESSVERTICES,
        APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETPALETTEENTRIES,
        APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSWAPCHAIN8_PRESENT,
        APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVOLUMETEXTURE8_UNLOCKBOX
    };

    for (const auto function : unsupported_array)
    {
        EXPECT_NE(D3D8FunctionPolicy::get_unsupported_reason_ptr(function),
                  nullptr) << Capture::APIDumpNames::get_function_name(function);
    }
}

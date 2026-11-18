/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the describers of the calls of OpenGL, Direct3D 7 and Direct3D 8.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_index/call_describer_set.h"

namespace
{
    Capture::APIDumpLoader::DumpedAPICall make_call(const APIInterceptor::APIFunction& in_function)
    {
        Capture::APIDumpLoader::DumpedAPICall result;

        result.func = in_function;

        return result;
    }
}

TEST(CallDescribers, DrawsAndClearsAreContentInEveryApi)
{
    using Analysis::APIDumpIndex::CallClass;
    using Analysis::APIDumpIndex::CallDescriberSet;

    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV),
              CallClass::CONTENT);
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS),
              CallClass::CONTENT);
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE),
              CallClass::CONTENT);
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR),
              CallClass::CONTENT);
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE),
              CallClass::CONTENT);
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_UPDATETEXTURE),
              CallClass::CONTENT);
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_UNLOCKRECT),
              CallClass::CONTENT);
}

TEST(CallDescribers, StateAndObjectCallsAreStateInEveryApi)
{
    using Analysis::APIDumpIndex::CallClass;
    using Analysis::APIDumpIndex::CallDescriberSet;

    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_GL_GLENABLE),
              CallClass::STATE);
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D),
              CallClass::STATE);
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE),
              CallClass::STATE);
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATESURFACE),
              CallClass::STATE);
    /* It copies the pixels, but it also creates the surface that later calls use: a dead frame must run it. */
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_DUPLICATESURFACE),
              CallClass::STATE);
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS),
              CallClass::STATE);
}

TEST(CallDescribers, ACallOfADisplayListIsConditionalAndACallOfSeveralIsState)
{
    using Analysis::APIDumpIndex::CallClass;
    using Analysis::APIDumpIndex::CallDescriberSet;

    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_GL_GLCALLLIST),
              CallClass::CONDITIONAL);
    EXPECT_EQ(CallDescriberSet::get_function_class(APIInterceptor::APIFUNCTION_GL_GLCALLLISTS),
              CallClass::STATE);
}

TEST(CallDescribers, TheCallsOfAListThatIsBeingRecordedAreState)
{
    using Analysis::APIDumpIndex::CallClass;

    Analysis::APIDumpIndex::CallDescriberSet set;

    EXPECT_EQ(set.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV) ),
              CallClass::CONTENT);
    EXPECT_EQ(set.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST) ),
              CallClass::STATE);
    EXPECT_EQ(set.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV) ),
              CallClass::STATE);
    EXPECT_EQ(set.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST) ),
              CallClass::STATE);
    EXPECT_EQ(set.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV) ),
              CallClass::CONTENT);
}

TEST(CallDescribers, ARecordingOfOpenGLDoesNotChangeWhatTheOtherApisAre)
{
    using Analysis::APIDumpIndex::CallClass;

    Analysis::APIDumpIndex::CallDescriberSet set;

    set.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST) );

    EXPECT_EQ(set.describe(make_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE) ),
              CallClass::CONTENT);
}

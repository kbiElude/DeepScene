/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what GLFunctionClassifier says: the kind of the functions that stand for each group, that the queries are those whose names say so, what the
 * analysis refuses and why, and that a function nobody follows is unknown.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_index/gl_function_classifier.h"
#include "capture/api_dump_names/function_names.h"
#include <string.h>

namespace
{
    using Analysis::APIDumpIndex::GLFunctionClassifier;
    using Analysis::APIDumpIndex::GLFunctionKind;
    using Analysis::APIDumpIndex::GLFunctionSupport;

    GLFunctionKind kind_of(const APIInterceptor::APIFunction& in_function)
    {
        return GLFunctionClassifier::classify(in_function).kind;
    }

    GLFunctionSupport support_of(const APIInterceptor::APIFunction& in_function)
    {
        return GLFunctionClassifier::classify(in_function).support;
    }
}

TEST(GLFunctionClassifier, TheCallsThatMakeAFrameAreContentAndTheCallOfAListIsConditional)
{
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLBEGIN),       GLFunctionKind::CONTENT);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLCLEAR),       GLFunctionKind::CONTENT);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS),  GLFunctionKind::CONTENT);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLREADPIXELS),  GLFunctionKind::CONTENT);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLVERTEX3F),    GLFunctionKind::CONTENT);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLCALLLIST),    GLFunctionKind::CONDITIONAL);
}

TEST(GLFunctionClassifier, TheValuesOfTheVerticesTheStateOfTheArraysAndTheNamesAreOfTheirOwnKinds)
{
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLCOLOR3F),          GLFunctionKind::CURRENT_VALUE);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLNORMAL3F),         GLFunctionKind::CURRENT_VALUE);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2F),       GLFunctionKind::CURRENT_VALUE);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD2F),  GLFunctionKind::CURRENT_VALUE);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLVERTEXPOINTER),    GLFunctionKind::CLIENT_ARRAY_STATE);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLENABLECLIENTSTATE), GLFunctionKind::CLIENT_ARRAY_STATE);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLGENTEXTURES),      GLFunctionKind::NAME_OR_SIZE);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLFLUSH),            GLFunctionKind::NAME_OR_SIZE);
}

TEST(GLFunctionClassifier, TheFunctionsWhoseArgumentsTheCurrentMatrixConvertsAreTheirOwnKindAndTheOtherLightCallsAreNot)
{
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLLIGHTFV),     GLFunctionKind::BAKES_CURRENT_MATRIX);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLCLIPPLANE),   GLFunctionKind::BAKES_CURRENT_MATRIX);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLRASTERPOS2F), GLFunctionKind::BAKES_CURRENT_MATRIX);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLLIGHTF),      GLFunctionKind::RENDER_STATE_UNTRACKED);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLFOGF),        GLFunctionKind::RENDER_STATE_UNTRACKED);
}

TEST(GLFunctionClassifier, AFunctionIsAQueryExactlyWhenItsNameStartsWithGetIsOrAre)
{
    size_t n_queries = 0;

    for (uint32_t n_function = static_cast<uint32_t>(APIInterceptor::APIFUNCTION_GL_FIRST);
                  n_function <= static_cast<uint32_t>(APIInterceptor::APIFUNCTION_GL_LAST);
                ++n_function)
    {
        const auto  function         = static_cast<APIInterceptor::APIFunction>(n_function);
        const char* name_ptr         = Capture::APIDumpNames::get_function_name(function);
        const bool  has_query_name   = (::strncmp(name_ptr, "glGet", 5) == 0 ||
                                        ::strncmp(name_ptr, "glIs",  4) == 0 ||
                                        ::strncmp(name_ptr, "glAre", 5) == 0);

        EXPECT_EQ(kind_of(function) == GLFunctionKind::QUERY,
                  has_query_name) << name_ptr;

        if (has_query_name)
        {
            n_queries++;
        }
    }

    EXPECT_GT(n_queries,
              100u);
}

TEST(GLFunctionClassifier, TheStateThatTheFrontEndFollowsIsStateAndSupported)
{
    const APIInterceptor::APIFunction function_array[] =
    {
        APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE,
        APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC,
        APIInterceptor::APIFUNCTION_GL_GLENABLE,
        APIInterceptor::APIFUNCTION_GL_GLFRONTFACE,
        APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE,
        APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
        APIInterceptor::APIFUNCTION_GL_GLPOPCLIENTATTRIB,
        APIInterceptor::APIFUNCTION_GL_GLPUSHCLIENTATTRIB,
        APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC,
        APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D
    };

    for (const auto function : function_array)
    {
        EXPECT_EQ(kind_of(function),
                  GLFunctionKind::STATE)    << Capture::APIDumpNames::get_function_name(function);
        EXPECT_EQ(support_of(function),
                  GLFunctionSupport::SUPPORTED) << Capture::APIDumpNames::get_function_name(function);
    }
}

TEST(GLFunctionClassifier, WhatTheAnalysisRefusesIsToldByTheReasonAndStaysAKnownFunction)
{
    EXPECT_EQ(support_of(APIInterceptor::APIFUNCTION_GL_GLLOGICOP),            GLFunctionSupport::UNSUPPORTED_DRAW_WRITE_STATE);
    EXPECT_EQ(support_of(APIInterceptor::APIFUNCTION_GL_GLBLENDEQUATION),      GLFunctionSupport::UNSUPPORTED_DRAW_WRITE_STATE);
    EXPECT_EQ(support_of(APIInterceptor::APIFUNCTION_GL_GLBITMAP),             GLFunctionSupport::UNSUPPORTED_FRAMEBUFFER_WRITE);
    EXPECT_EQ(support_of(APIInterceptor::APIFUNCTION_GL_GLDRAWPIXELS),         GLFunctionSupport::UNSUPPORTED_FRAMEBUFFER_WRITE);
    EXPECT_EQ(support_of(APIInterceptor::APIFUNCTION_GL_GLCOPYPIXELS),         GLFunctionSupport::UNSUPPORTED_FRAMEBUFFER_WRITE);
    EXPECT_EQ(support_of(APIInterceptor::APIFUNCTION_GL_GLCALLLISTS),          GLFunctionSupport::UNSUPPORTED_FRAMEBUFFER_WRITE);
    EXPECT_EQ(support_of(APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE3D),         GLFunctionSupport::UNSUPPORTED_TEXTURE);
    EXPECT_EQ(support_of(APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXIMAGE2D), GLFunctionSupport::UNSUPPORTED_TEXTURE);

    /* What a call is does not change with whether the analysis follows it: a bitmap is still content, a logic op still a state call. */
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLBITMAP),   GLFunctionKind::CONTENT);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLLOGICOP),  GLFunctionKind::STATE);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLCALLLISTS), GLFunctionKind::STATE);
}

TEST(GLFunctionClassifier, OnlyContentAndStateCallsAreRefusedByTheAnalysis)
{
    for (uint32_t n_function = static_cast<uint32_t>(APIInterceptor::APIFUNCTION_GL_FIRST);
                  n_function <= static_cast<uint32_t>(APIInterceptor::APIFUNCTION_GL_LAST);
                ++n_function)
    {
        const auto function = static_cast<APIInterceptor::APIFunction>(n_function);

        if (support_of(function) != GLFunctionSupport::SUPPORTED)
        {
            const auto kind = kind_of(function);

            EXPECT_TRUE(kind == GLFunctionKind::CONTENT ||
                        kind == GLFunctionKind::STATE) << Capture::APIDumpNames::get_function_name(function);
        }
    }
}

TEST(GLFunctionClassifier, AFunctionThatNoLayerFollowsIsUnknown)
{
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLBEGINQUERY),    GLFunctionKind::UNKNOWN);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLCREATESHADER),  GLFunctionKind::UNKNOWN);
    EXPECT_EQ(kind_of(APIInterceptor::APIFUNCTION_GL_GLDRAWBUFFER),    GLFunctionKind::UNKNOWN);
}

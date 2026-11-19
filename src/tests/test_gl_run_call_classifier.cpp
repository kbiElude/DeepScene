/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the right eye does about the calls of an OpenGL application while a run is kept (see GLRunCallClassifier): the calls that are kept, the calls that need
 * nothing, and everything else, which makes the run play first. The rule for everything else is checked against every OpenGL function there is, so a function
 * that is added to the interceptor cannot become one that the run skips by accident.
 */
#include <gtest/gtest.h>
#include "capture/api_dump_loader/api_info.h"
#include "capture/api_dump_names/function_names.h"
#include "common/api_function_argument.h"
#include "OpenGL/gl_run_call_classifier.h"
#include <set>
#include <string>

namespace
{
    using Stereo::ConverterDLL::GLRunCallClassifier;
    using Stereo::ConverterDLL::RunCallClass;

    const std::set<APIInterceptor::APIFunction> MATRIX_FUNCTION_SET =
    {
        APIInterceptor::APIFUNCTION_GL_GLFRUSTUM,
        APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY,
        APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD,
        APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF,
        APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE,
        APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXD,
        APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF,
        APIInterceptor::APIFUNCTION_GL_GLORTHO,
        APIInterceptor::APIFUNCTION_GL_GLPOPMATRIX,
        APIInterceptor::APIFUNCTION_GL_GLPUSHMATRIX,
        APIInterceptor::APIFUNCTION_GL_GLROTATED,
        APIInterceptor::APIFUNCTION_GL_GLROTATEF,
        APIInterceptor::APIFUNCTION_GL_GLSCALED,
        APIInterceptor::APIFUNCTION_GL_GLSCALEF,
        APIInterceptor::APIFUNCTION_GL_GLTRANSLATED,
        APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF
    };

    const std::set<APIInterceptor::APIFunction> RECORDED_FUNCTION_SET =
    {
        APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE,
        APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC,
        APIInterceptor::APIFUNCTION_GL_GLCOLORMASK,
        APIInterceptor::APIFUNCTION_GL_GLCULLFACE,
        APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC,
        APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK,
        APIInterceptor::APIFUNCTION_GL_GLDISABLE,
        APIInterceptor::APIFUNCTION_GL_GLENABLE,
        APIInterceptor::APIFUNCTION_GL_GLFRONTFACE,
        APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC,
        APIInterceptor::APIFUNCTION_GL_GLSTENCILOP,
        APIInterceptor::APIFUNCTION_GL_GLTEXENVI
    };

    std::string get_short_name(const APIInterceptor::APIFunction& in_function)
    {
        const char* const name_ptr = Capture::APIDumpNames::get_function_name(in_function);
        std::string       name     = (name_ptr != nullptr) ? name_ptr
                                                           : "";
        const size_t      cut      = name.find_last_of(":. ");

        return (cut != std::string::npos) ? name.substr(cut + 1)
                                          : name;
    }

    APIInterceptor::APIFunctionArgument make_i32_argument(const int32_t& in_value)
    {
        return APIInterceptor::APIFunctionArgument::create_i32(in_value);
    }
}

TEST(GLRunCallClassifier, ABlockAndItsEndNeedNothingFromTheRun)
{
    EXPECT_EQ(GLRunCallClassifier::classify(APIInterceptor::APIFUNCTION_GL_GLBEGIN),
              RunCallClass::NEUTRAL);
    EXPECT_EQ(GLRunCallClassifier::classify(APIInterceptor::APIFUNCTION_GL_GLEND),
              RunCallClass::NEUTRAL);
}

TEST(GLRunCallClassifier, ACallThatChangesWhatTheRightEyeWouldSeeMakesTheRunPlayFirst)
{
    const APIInterceptor::APIFunction function_array[] =
    {
        APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
        APIInterceptor::APIFUNCTION_GL_GLCLEAR,
        APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D,
        APIInterceptor::APIFUNCTION_GL_GLDELETETEXTURES,
        APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS,
        APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
        APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB,
        APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB,
        APIInterceptor::APIFUNCTION_GL_GLSCISSOR,
        APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D,
        APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI,
        APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE2D,
        APIInterceptor::APIFUNCTION_GL_GLVIEWPORT,
        APIInterceptor::APIFUNCTION_WGL_WGLMAKECURRENT,
        APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS
    };

    for (const auto function : function_array)
    {
        EXPECT_EQ(GLRunCallClassifier::classify(function),
                  RunCallClass::FLUSH)
            << get_short_name(function);
    }
}

TEST(GLRunCallClassifier, ACallThatAsksTheDriverForSomethingIsAnsweredFromTheLeftEye)
{
    const APIInterceptor::APIFunction function_array[] =
    {
        APIInterceptor::APIFUNCTION_GL_GLGETBOOLEANV,
        APIInterceptor::APIFUNCTION_GL_GLGETERROR,
        APIInterceptor::APIFUNCTION_GL_GLGETFLOATV,
        APIInterceptor::APIFUNCTION_GL_GLGETINTEGERV,
        APIInterceptor::APIFUNCTION_GL_GLISENABLED
    };

    for (const auto function : function_array)
    {
        EXPECT_EQ(GLRunCallClassifier::classify(function),
                  RunCallClass::QUERY)
            << get_short_name(function);
    }
}

TEST(GLRunCallClassifier, AFunctionOfAnotherApiNeedsNothingFromTheRun)
{
    EXPECT_EQ(GLRunCallClassifier::classify(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE),
              RunCallClass::NEUTRAL);
}

TEST(GLRunCallClassifier, EveryOpenGLFunctionThatIsNotListedHereMakesTheRunPlayFirst)
{
    uint32_t n_recorded = 0;

    for (uint32_t n_function = 0;
                  n_function < APIInterceptor::APIFUNCTION_COUNT;
                ++n_function)
    {
        const auto function = static_cast<APIInterceptor::APIFunction>(n_function);

        if (Capture::APIDumpLoader::get_api_group(function) != Capture::APIDumpLoader::APIGroup::GL)
        {
            continue;
        }

        const RunCallClass call_class = GLRunCallClassifier::classify(function);
        const std::string  name       = get_short_name(function);

        if (function == APIInterceptor::APIFUNCTION_GL_GLBEGIN ||
            function == APIInterceptor::APIFUNCTION_GL_GLEND)
        {
            EXPECT_EQ(call_class,
                      RunCallClass::NEUTRAL)
                << name;
        }
        else
        if (MATRIX_FUNCTION_SET.count(function) != 0)
        {
            EXPECT_EQ(call_class,
                      RunCallClass::MATRIX)
                << name;
        }
        else
        if (RECORDED_FUNCTION_SET.count(function) != 0)
        {
            EXPECT_EQ(call_class,
                      RunCallClass::RECORDED)
                << name;

            n_recorded++;
        }
        else
        if (name.compare(0, 5, "glGet") == 0 ||
            name.compare(0, 4, "glIs")  == 0)
        {
            EXPECT_EQ(call_class,
                      RunCallClass::QUERY)
                << name;
        }
        else
        {
            EXPECT_EQ(call_class,
                      RunCallClass::FLUSH)
                << name;
        }
    }

    EXPECT_EQ(n_recorded,
              RECORDED_FUNCTION_SET.size() );
}

TEST(GLRunCallClassifier, TheNumbersOfAStateCallAreReadInTheOrderOfItsArguments)
{
    const APIInterceptor::APIFunctionArgument argument_array[] =
    {
        make_i32_argument(0x0B90),
        make_i32_argument(-1),
        make_i32_argument(255)
    };

    Stereo::ConverterDLL::RunStateCall call;

    ASSERT_TRUE(GLRunCallClassifier::read_state_call(APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC,
                                               3,
                                               argument_array,
                                              &call) );

    EXPECT_EQ(call.function,
              APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC);
    EXPECT_EQ(call.value_i32vec4.at(0),
              0x0B90);
    EXPECT_EQ(call.value_i32vec4.at(1),
              -1);
    EXPECT_EQ(call.value_i32vec4.at(2),
              255);
}

TEST(GLRunCallClassifier, AStateCallWithFewerArgumentsThanItNeedsIsNotKept)
{
    const APIInterceptor::APIFunctionArgument argument_array[] =
    {
        make_i32_argument(0x0DE1)
    };

    Stereo::ConverterDLL::RunStateCall call;

    EXPECT_FALSE(GLRunCallClassifier::read_state_call(APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE,
                                                1,
                                                argument_array,
                                               &call) );
}


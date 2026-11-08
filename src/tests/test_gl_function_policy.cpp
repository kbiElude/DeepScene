/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for GlFunctionPolicy: which GL functions that the front end does not follow it may leave out. The ones that a real workload (Gerbera) makes must be among
 * them, and the ones that change what the model finds, or that it has never been told about, must not.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_function_policy.h"
#include "capture/api_dump_names/function_names.h"
#include <set>
#include <string>

namespace
{
    typedef Analysis::APIDumpAnalysis::Frontends::GlFunctionPolicy GlFunctionPolicy;
}

TEST(GlFunctionPolicy, TheFunctionsThatGerberaMakesAndTheFrontEndDoesNotFollowMayBeLeftOut)
{
    const APIInterceptor::APIFunction function_array[] =
    {
        APIInterceptor::APIFUNCTION_GL_GLCLIENTACTIVETEXTURE,
        APIInterceptor::APIFUNCTION_GL_GLCOLOR3FV,
        APIInterceptor::APIFUNCTION_GL_GLCOLOR3UB,
        APIInterceptor::APIFUNCTION_GL_GLCOLOR4F,
        APIInterceptor::APIFUNCTION_GL_GLCOLOR4FV,
        APIInterceptor::APIFUNCTION_GL_GLCOLOR4UB,
        APIInterceptor::APIFUNCTION_GL_GLDISABLECLIENTSTATE,
        APIInterceptor::APIFUNCTION_GL_GLENABLECLIENTSTATE,
        APIInterceptor::APIFUNCTION_GL_GLFLUSH,
        APIInterceptor::APIFUNCTION_GL_GLFOGF,
        APIInterceptor::APIFUNCTION_GL_GLFOGFV,
        APIInterceptor::APIFUNCTION_GL_GLFOGI,
        APIInterceptor::APIFUNCTION_GL_GLGENLISTS,
        APIInterceptor::APIFUNCTION_GL_GLGENTEXTURES,
        APIInterceptor::APIFUNCTION_GL_GLGETERROR,
        APIInterceptor::APIFUNCTION_GL_GLGETFLOATV,
        APIInterceptor::APIFUNCTION_GL_GLGETSTRING,
        APIInterceptor::APIFUNCTION_GL_GLLIGHTF,
        APIInterceptor::APIFUNCTION_GL_GLLIGHTMODELI,
        APIInterceptor::APIFUNCTION_GL_GLMATERIALFV,
        APIInterceptor::APIFUNCTION_GL_GLMATERIALI,
        APIInterceptor::APIFUNCTION_GL_GLNORMAL3F,
        APIInterceptor::APIFUNCTION_GL_GLNORMALPOINTER,
        APIInterceptor::APIFUNCTION_GL_GLPOLYGONOFFSET,
        APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2F,
        APIInterceptor::APIFUNCTION_GL_GLTEXCOORDPOINTER,
        APIInterceptor::APIFUNCTION_GL_GLTEXENVF,
        APIInterceptor::APIFUNCTION_GL_GLTEXENVI,
        APIInterceptor::APIFUNCTION_GL_GLTEXGENI,
        APIInterceptor::APIFUNCTION_GL_GLVERTEXPOINTER
    };

    for (const auto function : function_array)
    {
        EXPECT_TRUE(GlFunctionPolicy::is_left_out_by_design(function) ) << Capture::APIDumpNames::get_function_name(function);
    }
}

TEST(GlFunctionPolicy, WhatIsMadeRelativeToTheMatrixOfTheMomentIsNotLeftOutAndIsToldAsSuch)
{
    const APIInterceptor::APIFunction function_array[] =
    {
        APIInterceptor::APIFUNCTION_GL_GLCLIPPLANE,
        APIInterceptor::APIFUNCTION_GL_GLLIGHTFV,       /* the position and the direction of a light, which Gerbera gives */
        APIInterceptor::APIFUNCTION_GL_GLLIGHTIV,
        APIInterceptor::APIFUNCTION_GL_GLRASTERPOS3F,
        APIInterceptor::APIFUNCTION_GL_GLRASTERPOS4SV,
        APIInterceptor::APIFUNCTION_GL_GLTEXGENDV,      /* the planes: the forms that take an array */
        APIInterceptor::APIFUNCTION_GL_GLTEXGENIV
    };

    for (const auto function : function_array)
    {
        EXPECT_TRUE (GlFunctionPolicy::bakes_current_matrix(function) )    << Capture::APIDumpNames::get_function_name(function);
        EXPECT_FALSE(GlFunctionPolicy::is_left_out_by_design(function) )   << Capture::APIDumpNames::get_function_name(function);
    }
}

TEST(GlFunctionPolicy, WhatDoesNotDependOnTheMatrixIsNotToldAsSuch)
{
    const APIInterceptor::APIFunction function_array[] =
    {
        APIInterceptor::APIFUNCTION_GL_GLLIGHTF,        /* a scalar of a light: an attenuation, an angle */
        APIInterceptor::APIFUNCTION_GL_GLMATERIALFV,
        APIInterceptor::APIFUNCTION_GL_GLTEXGENI,       /* a mode, not a plane */
        APIInterceptor::APIFUNCTION_GL_GLTEXENVF,
        APIInterceptor::APIFUNCTION_GL_GLFOGFV,
        APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS3F    /* in the window, not through the matrix */
    };

    for (const auto function : function_array)
    {
        EXPECT_FALSE(GlFunctionPolicy::bakes_current_matrix(function) ) << Capture::APIDumpNames::get_function_name(function);
    }
}

TEST(GlFunctionPolicy, TheQueriesAreLeftOutWhateverTheyAre)
{
    EXPECT_TRUE(GlFunctionPolicy::is_left_out_by_design(APIInterceptor::APIFUNCTION_GL_GLGETINTEGERV) );
    EXPECT_TRUE(GlFunctionPolicy::is_left_out_by_design(APIInterceptor::APIFUNCTION_GL_GLGETTEXIMAGE) );
    EXPECT_TRUE(GlFunctionPolicy::is_left_out_by_design(APIInterceptor::APIFUNCTION_GL_GLISENABLED) );
    EXPECT_TRUE(GlFunctionPolicy::is_left_out_by_design(APIInterceptor::APIFUNCTION_GL_GLISTEXTURE) );
    EXPECT_TRUE(GlFunctionPolicy::is_left_out_by_design(APIInterceptor::APIFUNCTION_GL_GLARETEXTURESRESIDENT) );
}

TEST(GlFunctionPolicy, WhatChangesWhatTheModelFindsOrHasNeverBeenToldAboutIsNotLeftOut)
{
    const APIInterceptor::APIFunction function_array[] =
    {
        APIInterceptor::APIFUNCTION_GL_GLACCUM,              /* writes the accumulation buffer, which is of the framebuffer                       */
        APIInterceptor::APIFUNCTION_GL_GLARRAYELEMENT,       /* gives a vertex that the front end would not count                                 */
        APIInterceptor::APIFUNCTION_GL_GLCREATESHADER,       /* shaders                                                                           */
        APIInterceptor::APIFUNCTION_GL_GLDRAWBUFFER,         /* where a draw goes                                                                 */
        APIInterceptor::APIFUNCTION_GL_GLEVALMESH2,          /* makes geometry                                                                    */
        APIInterceptor::APIFUNCTION_GL_GLGENBUFFERS,         /* buffer objects                                                                    */
        APIInterceptor::APIFUNCTION_GL_GLPIXELTRANSFERF,     /* changes how the pixels of an upload are converted                                 */
        APIInterceptor::APIFUNCTION_GL_GLPOPCLIENTATTRIB,    /* gives back the state of how pixels are unpacked, which the front end keeps        */
        APIInterceptor::APIFUNCTION_GL_GLPUSHCLIENTATTRIB,
        APIInterceptor::APIFUNCTION_GL_GLREADBUFFER,         /* where what is copied to a texture comes from                                      */
        APIInterceptor::APIFUNCTION_GL_GLRECTF,              /* makes geometry                                                                    */
        APIInterceptor::APIFUNCTION_GL_GLRENDERMODE,         /* selection and feedback                                                            */
        APIInterceptor::APIFUNCTION_GL_GLUNIFORM1F,          /* the constants of a shader                                                         */
        APIInterceptor::APIFUNCTION_GL_GLUSEPROGRAM          /* a shader that draws are made with                                                 */
    };

    for (const auto function : function_array)
    {
        EXPECT_FALSE(GlFunctionPolicy::is_left_out_by_design(function) ) << Capture::APIDumpNames::get_function_name(function);
    }
}

TEST(GlFunctionPolicy, AFunctionThatTheFrontEndFollowsIsNotAskedAboutHere)
{
    /* The ones that the front end names are followed there; the policy says nothing of them, and they are not on its list. */
    EXPECT_FALSE(GlFunctionPolicy::is_left_out_by_design(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV) );
    EXPECT_FALSE(GlFunctionPolicy::is_left_out_by_design(APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D) );
    EXPECT_FALSE(GlFunctionPolicy::is_left_out_by_design(APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS) );
}

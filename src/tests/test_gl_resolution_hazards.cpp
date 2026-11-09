/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of which calls of an OpenGL application depend on the size of what it renders to in pixels in a way that a converter cannot make match.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_resolution_hazards.h"
#include <vector>

class GlResolutionHazardsTest : public ::testing::Test
{
protected:
    static bool classify(const APIInterceptor::APIFunction&                      in_function,
                         const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                         Analysis::APIDumpAnalysis::ResolutionHazard*            out_hazard_ptr)
    {
        return Analysis::APIDumpAnalysis::Frontends::GlResolutionHazards::classify(in_function,
                                                                                   static_cast<uint32_t>(in_argument_vec.size() ),
                                                                                   in_argument_vec.data(),
                                                                                   out_hazard_ptr);
    }

    static std::vector<APIInterceptor::APIFunctionArgument> make_enum_arguments(const std::vector<uint32_t>& in_value_vec)
    {
        std::vector<APIInterceptor::APIFunctionArgument> result_vec;

        for (const auto& value : in_value_vec)
        {
            result_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32_glenum(value) );
        }

        return result_vec;
    }
};

TEST_F(GlResolutionHazardsTest, WhatReadsOrDrawsPixelsAtPlacesInPixelsIsOne)
{
    struct HazardCase
    {
        APIInterceptor::APIFunction                 function;
        Analysis::APIDumpAnalysis::ResolutionHazard hazard;
    };

    const HazardCase case_array[] =
    {
        {APIInterceptor::APIFUNCTION_GL_GLBITMAP,     Analysis::APIDumpAnalysis::ResolutionHazard::RASTER_OPERATIONS},
        {APIInterceptor::APIFUNCTION_GL_GLCOPYPIXELS, Analysis::APIDumpAnalysis::ResolutionHazard::RASTER_OPERATIONS},
        {APIInterceptor::APIFUNCTION_GL_GLDRAWPIXELS, Analysis::APIDumpAnalysis::ResolutionHazard::RASTER_OPERATIONS},
        {APIInterceptor::APIFUNCTION_GL_GLPIXELZOOM,  Analysis::APIDumpAnalysis::ResolutionHazard::RASTER_OPERATIONS},
        {APIInterceptor::APIFUNCTION_GL_GLGETTEXIMAGE, Analysis::APIDumpAnalysis::ResolutionHazard::PIXEL_READBACKS},
        {APIInterceptor::APIFUNCTION_GL_GLREADPIXELS, Analysis::APIDumpAnalysis::ResolutionHazard::PIXEL_READBACKS}
    };

    for (const auto& hazard_case : case_array)
    {
        Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES;

        EXPECT_TRUE(classify(hazard_case.function,
                             {},
                            &hazard) ) << static_cast<int>(hazard_case.function);
        EXPECT_EQ  (hazard,
                    hazard_case.hazard);
    }
}

TEST_F(GlResolutionHazardsTest, AskingForTheViewportTheScissorBoxTheWidthOfLinesOrTheSizeOfPointsIsOneAndAskingForAMatrixIsNot)
{
    Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::PIXEL_READBACKS;

    for (const uint32_t name : {0x0BA2u, 0x0C10u, 0x0B21u, 0x0B11u})
    {
        EXPECT_TRUE(classify(APIInterceptor::APIFUNCTION_GL_GLGETINTEGERV,
                             make_enum_arguments({name}),
                            &hazard) ) << name;
        EXPECT_EQ  (hazard,
                    Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES);
    }

    /* GL_MODELVIEW_MATRIX and GL_PROJECTION_MATRIX say nothing of pixels. */
    EXPECT_FALSE(classify(APIInterceptor::APIFUNCTION_GL_GLGETFLOATV,
                          make_enum_arguments({0x0BA6u}),
                         &hazard) );
    EXPECT_FALSE(classify(APIInterceptor::APIFUNCTION_GL_GLGETFLOATV,
                          make_enum_arguments({0x0BA7u}),
                         &hazard) );
}

TEST_F(GlResolutionHazardsTest, AskingForTheSizeOfALevelOfATextureIsOneAndAskingForItsFormatIsNot)
{
    Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::PIXEL_READBACKS;

    /* The target, the level, the name of what is asked, where it goes. */
    EXPECT_TRUE (classify(APIInterceptor::APIFUNCTION_GL_GLGETTEXLEVELPARAMETERIV,
                          make_enum_arguments({0x0DE1u, 0u, 0x1000u}),
                         &hazard) );
    EXPECT_EQ   (hazard,
                 Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES);
    EXPECT_FALSE(classify(APIInterceptor::APIFUNCTION_GL_GLGETTEXLEVELPARAMETERIV,
                          make_enum_arguments({0x0DE1u, 0u, 0x1003u}),
                         &hazard) );
}

TEST_F(GlResolutionHazardsTest, WhatTheConverterMakesMatchIsNotOne)
{
    Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::PIXEL_READBACKS;

    for (const auto function : {APIInterceptor::APIFUNCTION_GL_GLVIEWPORT,
                                APIInterceptor::APIFUNCTION_GL_GLSCISSOR,
                                APIInterceptor::APIFUNCTION_GL_GLLINEWIDTH,
                                APIInterceptor::APIFUNCTION_GL_GLPOINTSIZE,
                                APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D,
                                APIInterceptor::APIFUNCTION_GL_GLCOPYTEXIMAGE2D,
                                APIInterceptor::APIFUNCTION_GL_GLCLEAR})
    {
        EXPECT_FALSE(classify(function,
                              {},
                             &hazard) ) << static_cast<int>(function);
    }
}

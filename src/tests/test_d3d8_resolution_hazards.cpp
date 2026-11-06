/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of which calls of a Direct3D 8 application depend on the size of what it renders to in pixels.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/d3d8/d3d8_resolution_hazards.h"
#include "Interception/Hooks/D3D8/types.h"
#include <vector>

class D3D8ResolutionHazardsTest : public ::testing::Test
{
protected:
    static bool classify(const APIInterceptor::APIFunction&                      in_function,
                         const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                         Analysis::APIDumpAnalysis::ResolutionHazard*                      out_hazard_ptr)
    {
        return Analysis::APIDumpAnalysis::Frontends::D3D8ResolutionHazards::classify(in_function,
                                                                                     static_cast<uint32_t>(in_argument_vec.size() ),
                                                                                     in_argument_vec.data(),
                                                                                     out_hazard_ptr);
    }

    /* The device (or the object the method is of), and then what the test gives. */
    static std::vector<APIInterceptor::APIFunctionArgument> make_arguments(const std::vector<uint32_t>& in_value_vec)
    {
        std::vector<APIInterceptor::APIFunctionArgument> result_vec;

        result_vec.push_back(APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr) );

        for (const auto& value : in_value_vec)
        {
            result_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(value) );
        }

        return result_vec;
    }
};

TEST_F(D3D8ResolutionHazardsTest, AClearIsOneOnlyIfItClearsRectangles)
{
    Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES;

    EXPECT_FALSE(classify(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR,
                          make_arguments({0}),
                         &hazard) );
    EXPECT_TRUE (classify(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR,
                          make_arguments({2}),
                         &hazard) );
    EXPECT_EQ   (hazard,
                 Analysis::APIDumpAnalysis::ResolutionHazard::CLEARS_WITH_RECTANGLES);
}

TEST_F(D3D8ResolutionHazardsTest, CopiesOfRectanglesShadersAndQueriesOfSizesAreEachOne)
{
    struct HazardCase
    {
        APIInterceptor::APIFunction                 function;
        Analysis::APIDumpAnalysis::ResolutionHazard hazard;
    };

    const HazardCase case_array[] =
    {
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_COPYRECTS,         Analysis::APIDumpAnalysis::ResolutionHazard::COPIES_WITH_RECTANGLES},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEPIXELSHADER, Analysis::APIDumpAnalysis::ResolutionHazard::SHADER_CREATIONS},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVERTEXSHADER, Analysis::APIDumpAnalysis::ResolutionHazard::SHADER_CREATIONS},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETVIEWPORT,       Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_GETDESC,          Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES}
    };

    for (const auto& hazard_case : case_array)
    {
        Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::CLEARS_WITH_RECTANGLES;

        EXPECT_TRUE(classify(hazard_case.function,
                             make_arguments({}),
                            &hazard) );
        EXPECT_EQ  (hazard,
                    hazard_case.hazard);
    }
}

TEST_F(D3D8ResolutionHazardsTest, OnlyTheStatesOfPointsAreHazardsOfRenderStates)
{
    Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES;

    EXPECT_TRUE (classify(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERSTATE,
                          make_arguments({static_cast<uint32_t>(Interception::Hooks::D3D8::D3DRS_POINTSIZE), 1}),
                         &hazard) );
    EXPECT_EQ   (hazard,
                 Analysis::APIDumpAnalysis::ResolutionHazard::POINT_SIZE_STATES);
    EXPECT_FALSE(classify(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERSTATE,
                          make_arguments({static_cast<uint32_t>(Interception::Hooks::D3D8::D3DRS_ZENABLE), 1}),
                         &hazard) );
}

TEST_F(D3D8ResolutionHazardsTest, AVertexFormatWithPositionsInPixelsIsOneButAShaderHandleIsNot)
{
    constexpr uint32_t DIFFUSE_COLOR_FORMAT = 0x040;
    constexpr uint32_t HANDLE_BIT           = 0x80000000u;
    constexpr uint32_t WORLD_POSITION_FORMAT = 0x002 | 0x010 | 0x100; /* position, normal, one pair of coordinates */

    Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES;

    EXPECT_TRUE (classify(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADER,
                          make_arguments({static_cast<uint32_t>(Interception::Hooks::D3D8::D3DFVF_XYZRHW) | DIFFUSE_COLOR_FORMAT}),
                         &hazard) );
    EXPECT_EQ   (hazard,
                 Analysis::APIDumpAnalysis::ResolutionHazard::SCREEN_SPACE_VERTICES);
    EXPECT_FALSE(classify(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADER,
                          make_arguments({WORLD_POSITION_FORMAT}),
                         &hazard) );
    EXPECT_FALSE(classify(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADER,
                          make_arguments({HANDLE_BIT | static_cast<uint32_t>(Interception::Hooks::D3D8::D3DFVF_XYZRHW)}),
                         &hazard) );
}

TEST_F(D3D8ResolutionHazardsTest, AnythingElseIsNotAHazard)
{
    Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES;

    EXPECT_FALSE(classify(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE,
                          make_arguments({4, 0, 1}),
                         &hazard) );
    EXPECT_FALSE(classify(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT,
                          make_arguments({}),
                         &hazard) );
}

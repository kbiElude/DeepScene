/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what the state effect describers say of calls: the table of OpenGL, display lists, the window system and Direct3D, and that a
 * function that no describer knows ends the process.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_index/gl_state_effect_describer.h"
#include "analysis/api_dump_index/state_effect_describer_set.h"
#include "common/macros.h"
#include <stdlib.h>

namespace
{
    using Capture::APIDumpLoader::DumpedAPICall;
    using Analysis::APIDumpIndex::StateEffect;
    using Analysis::APIDumpIndex::StateEffectKind;

    constexpr uint32_t GL_AMBIENT            = 0x1200;
    constexpr uint32_t GL_COLOR_MATERIAL     = 0x0B57;
    constexpr uint32_t GL_COMPILE            = 0x1300;
    constexpr uint32_t GL_COMPILE_AND_EXECUTE = 0x1301;
    constexpr uint32_t GL_DEPTH_TEST         = 0x0B71;
    constexpr uint32_t GL_POSITION           = 0x1203;
    constexpr uint32_t GL_TEXTURE_2D         = 0x0DE1;
    constexpr uint32_t GL_TEXTURE_COORD_ARRAY = 0x8078;
    constexpr uint32_t GL_VERTEX_ARRAY       = 0x8074;
    constexpr uint32_t ACTIVE_TEXTURE_BIT    = 1u << 0;
    constexpr uint32_t CLIENT_ACTIVE_BIT     = 1u << 1;
    constexpr uint32_t MATRIX_MODE_BIT       = 1u << 2;

    DumpedAPICall make_call(const APIInterceptor::APIFunction& in_function,
                            const std::vector<uint32_t>&       in_argument_vec = std::vector<uint32_t>() )
    {
        DumpedAPICall result;

        result.func = in_function;

        for (const auto& value : in_argument_vec)
        {
            result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(value) );
        }

        return result;
    }

    void set_no_error_dialog()
    {
        ::_putenv_s("APIINTERCEPTOR_NO_ERROR_DIALOG",
                    "1");
    }
}

TEST(GLStateEffectDescriber, EnablingAndDisablingTheSameThingSetTheSamePieceOfState)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    const StateEffect enable  = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                                             {GL_DEPTH_TEST}) );
    const StateEffect disable = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLDISABLE,
                                                             {GL_DEPTH_TEST}) );

    EXPECT_EQ(enable.kind,
              StateEffectKind::ASSIGN);
    EXPECT_EQ(enable.key,
              disable.key);
    EXPECT_EQ(enable.selector_mask,
              0u);
}

TEST(GLStateEffectDescriber, AFlagOfTheTextureUnitDependsOnTheActiveUnitAndTheOneOfMaterialsIsABarrier)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    const StateEffect unit_flag = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                                               {GL_TEXTURE_2D}) );
    const StateEffect material  = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                                               {GL_COLOR_MATERIAL}) );

    EXPECT_EQ(unit_flag.kind,
              StateEffectKind::ASSIGN);
    EXPECT_EQ(unit_flag.selector_mask,
              ACTIVE_TEXTURE_BIT);
    EXPECT_EQ(material.kind,
              StateEffectKind::BARRIER);
}

TEST(GLStateEffectDescriber, AClientStateOfATextureUnitDependsOnTheClientUnitAndAnUnknownOneIsABarrier)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    const StateEffect plain   = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLENABLECLIENTSTATE,
                                                             {GL_VERTEX_ARRAY}) );
    const StateEffect texture = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLENABLECLIENTSTATE,
                                                             {GL_TEXTURE_COORD_ARRAY}) );
    const StateEffect unknown = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLENABLECLIENTSTATE,
                                                             {0x1234}) );

    EXPECT_EQ(plain.kind,
              StateEffectKind::ASSIGN);
    EXPECT_EQ(plain.selector_mask,
              0u);
    EXPECT_EQ(texture.selector_mask,
              CLIENT_ACTIVE_BIT);
    EXPECT_EQ(unknown.kind,
              StateEffectKind::BARRIER);
}

TEST(GLStateEffectDescriber, TheCallsThatChooseAreSelectorsWithTheValueTheyChoose)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    const StateEffect unit   = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE,
                                                            {0x84C1}) );
    const StateEffect client = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLCLIENTACTIVETEXTURE,
                                                            {0x84C2}) );
    const StateEffect mode   = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE,
                                                            {0x1701}) );

    EXPECT_EQ(unit.kind,
              StateEffectKind::SELECT);
    EXPECT_EQ(unit.selector_value,
              0x84C1u);
    EXPECT_EQ(client.selector_value,
              0x84C2u);
    EXPECT_EQ(mode.selector_value,
              0x1701u);
    EXPECT_NE(unit.selector,
              client.selector);
    EXPECT_NE(unit.selector,
              mode.selector);
    EXPECT_NE(client.selector,
              mode.selector);
}

TEST(GLStateEffectDescriber, TheMatrixCallsAreMadeInPlaceWithTheMatrixMode)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    for (const auto& function : {APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY,
                                 APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF,
                                 APIInterceptor::APIFUNCTION_GL_GLPUSHMATRIX,
                                 APIInterceptor::APIFUNCTION_GL_GLPOPMATRIX,
                                 APIInterceptor::APIFUNCTION_GL_GLORTHO,
                                 APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF})
    {
        const StateEffect effect = describer.describe(make_call(function) );

        EXPECT_EQ(effect.kind,
                  StateEffectKind::ORDERED);
        EXPECT_EQ(effect.selector_mask,
                  MATRIX_MODE_BIT);
    }
}

TEST(GLStateEffectDescriber, ABindReadsTheDomainOfTheParametersOfTexturesAndUsesTheActiveUnit)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    const StateEffect bind      = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE,
                                                               {GL_TEXTURE_2D, 1}) );
    const StateEffect parameter = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI,
                                                               {GL_TEXTURE_2D, 0x2801, 0x2601}) );

    EXPECT_EQ(bind.kind,
              StateEffectKind::ORDERED);
    EXPECT_EQ(bind.selector_mask,
              ACTIVE_TEXTURE_BIT);
    EXPECT_EQ(parameter.kind,
              StateEffectKind::ASSIGN);
    EXPECT_EQ(parameter.selector_mask,
              ACTIVE_TEXTURE_BIT);

    /* What the bind reads is what the parameters write. */
    EXPECT_EQ(bind.domain_mask,
              parameter.domain_mask);
}

TEST(GLStateEffectDescriber, ALightPositionDependsOnTheMatrixAndOtherLightValuesDoNot)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    const StateEffect position = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLLIGHTFV,
                                                              {0x4000, GL_POSITION, 0}) );
    const StateEffect ambient  = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLLIGHTFV,
                                                              {0x4000, GL_AMBIENT, 0}) );
    const StateEffect other    = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLLIGHTFV,
                                                              {0x4001, GL_AMBIENT, 0}) );

    EXPECT_EQ(position.kind,
              StateEffectKind::ORDERED);
    EXPECT_EQ(ambient.kind,
              StateEffectKind::ASSIGN);
    EXPECT_NE(ambient.key,
              other.key);
}

TEST(GLStateEffectDescriber, TheCurrentColorIsABarrierBecauseItCanSetTheMaterial)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    for (const auto& function : {APIInterceptor::APIFUNCTION_GL_GLCOLOR3F,
                                 APIInterceptor::APIFUNCTION_GL_GLCOLOR4UB,
                                 APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
                                 APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB,
                                 APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D})
    {
        EXPECT_EQ(describer.describe(make_call(function) ).kind,
                  StateEffectKind::BARRIER);
    }
}

TEST(GLStateEffectDescriber, TheSaveAndTheRestoreOfClientAttributesAreBarriers)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    /* A pop gives back state that calls before the push set, so no call may be moved across either of them. */
    for (const auto& function : {APIInterceptor::APIFUNCTION_GL_GLPOPCLIENTATTRIB,
                                 APIInterceptor::APIFUNCTION_GL_GLPUSHCLIENTATTRIB})
    {
        EXPECT_EQ(describer.describe(make_call(function) ).kind,
                  StateEffectKind::BARRIER);
    }
}

TEST(GLStateEffectDescriber, TheCallsOfAListThatIsBeingMadeAreRecordedAndTheEndOfTheListIsABarrier)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    EXPECT_EQ(describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                                           {1, GL_COMPILE}) ).kind,
              StateEffectKind::BARRIER);

    /* Content as well as state, since a list is made of what is recorded into it. */
    EXPECT_EQ(describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                           {GL_DEPTH_TEST}) ).kind,
              StateEffectKind::RECORDED);
    EXPECT_EQ(describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV) ).kind,
              StateEffectKind::RECORDED);
    EXPECT_EQ(describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST) ).kind,
              StateEffectKind::BARRIER);

    EXPECT_EQ(describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                           {GL_DEPTH_TEST}) ).kind,
              StateEffectKind::ASSIGN);
}

TEST(GLStateEffectDescriber, AListThatIsMadeAndExecutedAtOnceEndsTheProcess)
{
    set_no_error_dialog();

    const auto body = []()
    {
        Analysis::APIDumpIndex::GLStateEffectDescriber describer;

        describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                                     {1, GL_COMPILE_AND_EXECUTE}) );
    };

    EXPECT_EXIT(body(),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "GL_COMPILE_AND_EXECUTE");
}

TEST(GLStateEffectDescriber, AContentCallOutsideAListEndsTheProcess)
{
    set_no_error_dialog();

    const auto body = []()
    {
        Analysis::APIDumpIndex::GLStateEffectDescriber describer;

        describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV) );
    };

    EXPECT_EXIT(body(),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "outside a display list");
}

TEST(GLStateEffectDescriber, TheFrontFaceTheColorMaskAndTheStencilCallsEachSetAPieceOfStateOfTheirOwn)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    const APIInterceptor::APIFunction function_array[] =
    {
        APIInterceptor::APIFUNCTION_GL_GLCOLORMASK,
        APIInterceptor::APIFUNCTION_GL_GLFRONTFACE,
        APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC,
        APIInterceptor::APIFUNCTION_GL_GLSTENCILMASK,
        APIInterceptor::APIFUNCTION_GL_GLSTENCILOP
    };

    std::vector<StateEffect> effect_vec;

    for (const auto function : function_array)
    {
        effect_vec.push_back(describer.describe(make_call(function,
                                                          {1, 1, 1, 1}) ) );

        EXPECT_EQ(effect_vec.back().kind,
                  StateEffectKind::ASSIGN) << static_cast<int>(function);
    }

    /* Two calls that set different state are not coalesced into one. */
    for (size_t n_first = 0;
                n_first < effect_vec.size();
              ++n_first)
    {
        for (size_t n_second = n_first + 1;
                    n_second < effect_vec.size();
                  ++n_second)
        {
            EXPECT_FALSE(effect_vec.at(n_first).key == effect_vec.at(n_second).key) << n_first << " " << n_second;
        }
    }
}

TEST(GLStateEffectDescriber, AQueryOfTheDriverAndAVideoUploadChangeNoTrackedStateExceptThatAnUploadIsABarrier)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    const StateEffect boolean_query = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLGETBOOLEANV) );
    const StateEffect integer_query = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLGETINTEGERV) );
    const StateEffect upload        = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE2D) );

    EXPECT_EQ(boolean_query.kind,
              StateEffectKind::ORDERED);
    EXPECT_EQ(boolean_query.domain_mask,
              0u);
    EXPECT_EQ(integer_query.kind,
              StateEffectKind::ORDERED);
    EXPECT_EQ(upload.kind,
              StateEffectKind::BARRIER);
}

TEST(GLStateEffectDescriber, ADoubleMatrixCallAndAFourComponentTextureCoordinateAreTreatedLikeTheirFloatForms)
{
    Analysis::APIDumpIndex::GLStateEffectDescriber describer;

    const StateEffect mult_double = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXD) );
    const StateEffect mult_float  = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF) );
    const StateEffect coord_four  = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLTEXCOORD4F) );
    const StateEffect coord_two   = describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2F) );

    EXPECT_EQ(mult_double.kind,
              mult_float.kind);
    EXPECT_EQ(mult_double.selector_mask,
              mult_float.selector_mask);
    EXPECT_EQ(coord_four.key,
              coord_two.key);
}

TEST(GLStateEffectDescriber, AFunctionThatIsNotInTheTableEndsTheProcess)
{
    set_no_error_dialog();

    const auto body = []()
    {
        Analysis::APIDumpIndex::GLStateEffectDescriber describer;

        describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLBEGINQUERY) );
    };

    EXPECT_EXIT(body(),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "Unsupported value");
}

TEST(GLStateEffectDescriber, AFunctionThatIsNotInTheTableEndsTheProcessInsideAListToo)
{
    set_no_error_dialog();

    const auto body = []()
    {
        Analysis::APIDumpIndex::GLStateEffectDescriber describer;

        describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                                     {1, GL_COMPILE}) );
        describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLBEGINQUERY) );
    };

    EXPECT_EXIT(body(),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "Unsupported value");
}

TEST(GLStateEffectDescriber, ACallWithoutTheNumberThatTellsWhatItSetsEndsTheProcess)
{
    set_no_error_dialog();

    const auto body = []()
    {
        Analysis::APIDumpIndex::GLStateEffectDescriber describer;

        describer.describe(make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE) );
    };

    EXPECT_EXIT(body(),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "no number");
}

TEST(StateEffectDescriberSet, TheWindowSystemAndDirect3DAreBarriers)
{
    Analysis::APIDumpIndex::StateEffectDescriberSet set;

    for (const auto& function : {APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS,
                                 APIInterceptor::APIFUNCTION_GDI32_SETPIXELFORMAT,
                                 APIInterceptor::APIFUNCTION_USER32_CREATEWINDOWEXA,
                                 APIInterceptor::APIFUNCTION_WGL_WGLMAKECURRENT,
                                 APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE,
                                 APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERSTATE})
    {
        EXPECT_EQ(set.describe(make_call(function) ).kind,
                  StateEffectKind::BARRIER);
    }
}

TEST(StateEffectDescriberSet, ACallOfAnApiThatHasNoDescriberEndsTheProcess)
{
    set_no_error_dialog();

    const auto body = []()
    {
        Analysis::APIDumpIndex::StateEffectDescriberSet set;

        set.describe(make_call(APIInterceptor::APIFUNCTION_UNKNOWN) );
    };

    EXPECT_EXIT(body(),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "no state effect describer");
}

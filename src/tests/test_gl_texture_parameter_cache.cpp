/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for GLTextureParameterCache: which glTexParameter calls of a replay it leaves out, and when it forgets what it knows.
 */
#include <gtest/gtest.h>
#include "gl_texture_parameter_cache.h"
#include <algorithm>

namespace
{
    constexpr uint32_t BORDER_COLOR = 0x1004; /* GL_TEXTURE_BORDER_COLOR: not one that the cache looks at */
    constexpr uint32_t LINEAR       = 0x2601;
    constexpr uint32_t MAG_FILTER   = 0x2800;
    constexpr uint32_t MIN_FILTER   = 0x2801;
    constexpr uint32_t NEAREST      = 0x2600;
    constexpr uint32_t TEXTURE0     = 0x84C0;
    constexpr uint32_t TEXTURE1     = 0x84C1;
    constexpr uint32_t TEXTURE_2D   = 0x0DE1;

    Capture::APIDumpLoader::DumpedAPICall make_call(const APIInterceptor::APIFunction& in_function,
                                                    const std::vector<uint32_t>&       in_number_vec = {})
    {
        Capture::APIDumpLoader::DumpedAPICall result;

        result.func = in_function;

        for (const auto number : in_number_vec)
        {
            result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(number) );
        }

        return result;
    }

    /* What the engine does with a call: asks, and says what came of it. Returns whether the call was made. */
    bool play(Stereo::Replay::EngineDLL::GLTextureParameterCache*  inout_cache_ptr,
              const Capture::APIDumpLoader::DumpedAPICall&         in_call,
              const uint32_t&                                      in_n_frame = 0)
    {
        const bool is_executed = inout_cache_ptr->should_execute_call(in_call,
                                                                       in_n_frame);

        inout_cache_ptr->on_call_replayed(in_call,
                                          in_n_frame,
                                          is_executed);

        return is_executed;
    }

    bool bind(Stereo::Replay::EngineDLL::GLTextureParameterCache* inout_cache_ptr,
              const uint32_t&                                     in_texture)
    {
        return play(inout_cache_ptr,
                    make_call(APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE,
                              {TEXTURE_2D, in_texture}) );
    }

    bool set_parameter(Stereo::Replay::EngineDLL::GLTextureParameterCache* inout_cache_ptr,
                       const uint32_t&                                     in_parameter,
                       const uint32_t&                                     in_value)
    {
        return play(inout_cache_ptr,
                    make_call(APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI,
                              {TEXTURE_2D, in_parameter, in_value}) );
    }
}

TEST(GLTextureParameterCache, AParameterThatHasTheValueIsLeftOutAndAChangedOneIsNot)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    bind(&cache, 7);

    EXPECT_TRUE (set_parameter(&cache, MIN_FILTER, LINEAR) );  /* the first: what the texture has is not known */
    EXPECT_FALSE(set_parameter(&cache, MIN_FILTER, LINEAR) );
    EXPECT_FALSE(set_parameter(&cache, MIN_FILTER, LINEAR) );
    EXPECT_TRUE (set_parameter(&cache, MIN_FILTER, NEAREST) );
    EXPECT_FALSE(set_parameter(&cache, MIN_FILTER, NEAREST) );
    EXPECT_TRUE (set_parameter(&cache, MIN_FILTER, LINEAR) );  /* back to what it was: it is a change */
    EXPECT_EQ   (cache.get_n_skipped_calls(), 3u);
}

TEST(GLTextureParameterCache, EachTextureAndEachParameterHasItsOwnValue)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    bind(&cache, 1);
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );
    EXPECT_TRUE(set_parameter(&cache, MAG_FILTER, LINEAR) );

    bind(&cache, 2);
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) ); /* another texture */

    bind(&cache, 1);
    EXPECT_FALSE(set_parameter(&cache, MIN_FILTER, LINEAR) );
    EXPECT_FALSE(set_parameter(&cache, MAG_FILTER, LINEAR) );

    bind(&cache, 2);
    EXPECT_FALSE(set_parameter(&cache, MIN_FILTER, LINEAR) );
    EXPECT_TRUE (set_parameter(&cache, MAG_FILTER, LINEAR) );
}

TEST(GLTextureParameterCache, EachUnitHasItsOwnBinding)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE0}) );
    bind(&cache, 1);
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE1}) );
    bind(&cache, 2);
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) ); /* texture 2, on the other unit */

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE0}) );
    EXPECT_FALSE(set_parameter(&cache, MIN_FILTER, LINEAR) ); /* texture 1 still */
}

TEST(GLTextureParameterCache, AParameterThatTheCacheDoesNotLookAtIsNeverLeftOut)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    bind(&cache, 1);

    EXPECT_TRUE(set_parameter(&cache, BORDER_COLOR, 0) );
    EXPECT_TRUE(set_parameter(&cache, BORDER_COLOR, 0) );
}

TEST(GLTextureParameterCache, AFloatThatIsTheValueOfAnIntegerIsTheSameValue)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;
    Capture::APIDumpLoader::DumpedAPICall              float_call;

    float_call.func = APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERF;

    float_call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(TEXTURE_2D) );
    float_call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(MIN_FILTER) );
    float_call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_fp32(static_cast<float>(LINEAR) ) );

    bind(&cache, 1);

    EXPECT_TRUE (set_parameter(&cache, MIN_FILTER, LINEAR) );
    EXPECT_FALSE(play(&cache, float_call) );
}

TEST(GLTextureParameterCache, NothingIsLeftOutBeforeATextureIsBound)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );
}

TEST(GLTextureParameterCache, DeletingTexturesMakesTheCacheForgetThem)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    bind(&cache, 1);
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLDELETETEXTURES, {1}) );

    /* The name is made again, and what it has is the default; nothing is bound until a texture is. */
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE0}) );
    bind(&cache, 1);

    EXPECT_TRUE (set_parameter(&cache, MIN_FILTER, LINEAR) );
    EXPECT_FALSE(set_parameter(&cache, MIN_FILTER, LINEAR) );
}

TEST(GLTextureParameterCache, GLPopAttribMakesTheCacheForgetEverything)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    bind(&cache, 1);
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB) );
    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE0}) );
    bind(&cache, 1);

    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );
}

TEST(GLTextureParameterCache, ACallThatAnotherInterceptorLeftOutMakesTheCacheForget)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    bind(&cache, 1);
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    const auto bind_call = make_call(APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE,
                                     {TEXTURE_2D, 2});

    /* The cache said yes, and someone else said no. */
    ASSERT_TRUE(cache.should_execute_call(bind_call,
                                          0) );
    cache.on_call_replayed(bind_call,
                           0,
                           false /* in_was_executed */);

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE0}) );
    bind(&cache, 1);

    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );
}

TEST(GLTextureParameterCache, TheCallsOfAListThatIsBeingMadeAreNeverLeftOut)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    bind(&cache, 1);
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST, {5}) );

    /* In the list, the parameter has the value: the list has the call all the same. */
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST) );
}

TEST(GLTextureParameterCache, ACallOfAListThatSetsTheTextureStateMakesTheCacheForgetButOneThatDoesNotDoesNot)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    /* List 5 sets a parameter; list 6 only draws. */
    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST, {5}) );
    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI, {TEXTURE_2D, MIN_FILTER, NEAREST}) );
    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST) );

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST, {6}) );
    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLBEGIN, {7}) );
    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLEND) );
    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST) );

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE0}) );
    bind(&cache, 1);
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST, {6}) );
    EXPECT_FALSE(set_parameter(&cache, MIN_FILTER, LINEAR) ); /* the list that only draws changed nothing */

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST, {5}) );

    /* The list may have changed the parameter, and the unit that is active, so nothing is known until a texture is bound again. */
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE0}) );
    bind(&cache, 1);

    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );
}

TEST(GLTextureParameterCache, AListThatIsNotKnownIsTakenForOneThatSetsTheTextureState)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    bind(&cache, 1);
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST, {99}) );
    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE0}) );
    bind(&cache, 1);

    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );
}

TEST(GLTextureParameterCache, AReplayThatStartsOverMakesTheCacheForget)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE0}), 40);
    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE, {TEXTURE_2D, 1}), 40);

    EXPECT_TRUE(play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI, {TEXTURE_2D, MIN_FILTER, LINEAR}), 40) );
    EXPECT_FALSE(play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI, {TEXTURE_2D, MIN_FILTER, LINEAR}), 41) );

    /* Frame 0 comes after frame 41: the replay started over, with a context that has nothing of what it had. */
    EXPECT_TRUE(play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI, {TEXTURE_2D, MIN_FILTER, LINEAR}), 0) );
}

TEST(GLTextureParameterCache, AnotherContextHasOtherTextures)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    const auto make_current = [](const uint64_t& in_context)
    {
        Capture::APIDumpLoader::DumpedAPICall result;

        result.func = APIInterceptor::APIFUNCTION_WGL_WGLMAKECURRENT;

        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u64(0x1000) );
        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u64(in_context) );

        return result;
    };

    play(&cache, make_current(0xA) );
    bind(&cache, 1);
    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    /* The same context again, and making none current in between, change nothing. */
    play(&cache, make_current(0) );
    play(&cache, make_current(0xA) );
    EXPECT_FALSE(set_parameter(&cache, MIN_FILTER, LINEAR) );

    play(&cache, make_current(0xB) );
    play(&cache, make_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE, {TEXTURE0}) );
    bind(&cache, 1);

    EXPECT_TRUE(set_parameter(&cache, MIN_FILTER, LINEAR) );
}

TEST(GLTextureParameterCache, TheParametersThatAreLookedAtAreThoseThatATwinTakes)
{
    const auto& parameter_vec = Stereo::Replay::EngineDLL::GLTextureParameterCache::get_cached_parameters();

    EXPECT_EQ(parameter_vec.size(), 10u);
    EXPECT_NE(std::find(parameter_vec.begin(), parameter_vec.end(), MIN_FILTER), parameter_vec.end() );
    EXPECT_EQ(std::find(parameter_vec.begin(), parameter_vec.end(), BORDER_COLOR), parameter_vec.end() );
}

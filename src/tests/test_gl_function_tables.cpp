/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The OpenGL functions are classified in more than one place (the analysis front end, and the state coalescing of the loader), and each place
 * ends the process for a function it does not know. These checks give every function to each of them, and say where they disagree: a function that
 * the front end follows (an analysis of a workload that calls it succeeds) must be one that the state coalescing knows too, or a seek in that workload
 * ends the process.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_function_policy.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/opengl_frontend.h"
#include "capture/api_dump_loader/gl_call_describer.h"
#include "capture/api_dump_loader/gl_state_effect_describer.h"
#include "capture/api_dump_names/function_names.h"
#include "common/fatal_error.h"
#include "frontend_harness.h"
#include "gl_call_builder.h"
#include <set>
#include <string>
#include <vector>

namespace
{
    class GLFunctionTables : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            APIInterceptor::FatalError::set_throws_for_tests(true);
        }

        void TearDown() override
        {
            APIInterceptor::FatalError::set_throws_for_tests(false);
        }

        /* Whether the front end goes on when a call of the function reaches it, in a program that has a context. It ends the process for a function that
         * it does not follow and cannot tell to be of no matter: that is the kind of stop that says "(the function)". A stop that has to do with the
         * arguments (the dummy ones here) says the function is followed, whatever it did to them.
         */
        static bool does_front_end_follow(const APIInterceptor::APIFunction& in_function)
        {
            constexpr uint64_t CONTEXT_ADDRESS        = 0x10000;
            constexpr uint64_t DEVICE_CONTEXT_ADDRESS = 0x63012365;

            Tests::GLCallBuilder                                                                      builder;
            Tests::FrontendHarness<Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend,
                                   Tests::GLCallBuilder>                                              harness;
            bool                                                                                      result = true;

            builder.make_current(DEVICE_CONTEXT_ADDRESS,
                                 CONTEXT_ADDRESS);
            builder.viewport    (0,
                                 0,
                                 640,
                                 480);
            builder.add_call    (in_function,
                                 {APIInterceptor::APIFunctionArgument::create_i32(0)});

            try
            {
                harness.feed(builder);
            }
            catch (const APIInterceptor::FatalError::Exception& in_exception)
            {
                result = (std::string(in_exception.what() ).find("(the function)") == std::string::npos);
            }

            return result;
        }

        /* Whether the state coalescing of the loader knows the function: it names the functions that are not content calls in its table, and ends the
         * process, with an unsupported value, for one that is not in it. Content calls (draws, clears, copies) are told by their class.
         */
        static bool does_state_coalescing_know(const APIInterceptor::APIFunction& in_function)
        {
            constexpr size_t N_ARGUMENTS = 6;

            Capture::APIDumpLoader::DumpedAPICall            call;
            Capture::APIDumpLoader::GLStateEffectDescriber   describer;
            bool                                             result = true;

            if (Capture::APIDumpLoader::GLCallDescriber::get_function_class(in_function) == Capture::APIDumpLoader::CallClass::CONTENT)
            {
                goto end;
            }

            call.func = in_function;

            for (size_t n_argument = 0;
                        n_argument < N_ARGUMENTS;
                      ++n_argument)
            {
                call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(0) );
            }

            try
            {
                describer.describe(call);
            }
            catch (const APIInterceptor::FatalError::Exception& in_exception)
            {
                result = (in_exception.is_unsupported_value()                                        == false ||
                          std::string(in_exception.what() ).find("in_call.func") == std::string::npos);
            }

        end:
            return result;
        }

        static std::vector<APIInterceptor::APIFunction> get_gl_functions()
        {
            std::vector<APIInterceptor::APIFunction> result;

            for (uint32_t n_function = static_cast<uint32_t>(APIInterceptor::APIFUNCTION_GL_FIRST);
                          n_function <= static_cast<uint32_t>(APIInterceptor::APIFUNCTION_GL_LAST);
                        ++n_function)
            {
                result.push_back(static_cast<APIInterceptor::APIFunction>(n_function) );
            }

            return result;
        }
    };
}

TEST_F(GLFunctionTables, EveryFunctionThatTheFrontEndFollowsIsKnownToTheStateCoalescingExceptThoseOfTheLedger)
{
    /* The functions on which the two disagree today (see gl_function_table_gaps.inl). The list may only get shorter: a new gap fails this test, and so does
     * a gap that was closed and is still listed.
     */
    const char* const ledger_array[] =
    {
#include "gl_function_table_gaps.inl"
    };

    std::set<std::string> ledger_set;
    std::set<std::string> missing_set;
    size_t                n_followed = 0;

    for (const auto name_ptr : ledger_array)
    {
        ledger_set.insert(name_ptr);
    }

    for (const auto function : get_gl_functions() )
    {
        if (does_front_end_follow(function) )
        {
            n_followed++;

            if (does_state_coalescing_know(function) == false)
            {
                missing_set.insert(Capture::APIDumpNames::get_function_name(function) );
            }
        }
    }

    std::string closed_names;
    std::string new_names;

    for (const auto& name : missing_set)
    {
        if (ledger_set.count(name) == 0)
        {
            new_names += name + " ";
        }
    }

    for (const auto& name : ledger_set)
    {
        if (missing_set.count(name) == 0)
        {
            closed_names += name + " ";
        }
    }

    /* Not a vacuous pass: a front end that followed nothing would have nothing to disagree about. */
    EXPECT_GT(n_followed,
              100u);

    EXPECT_TRUE(new_names.empty() )    << "followed by the front end, unknown to the state coalescing (GLStateEffectDescriber), and not in the ledger: " << new_names;
    EXPECT_TRUE(closed_names.empty() ) << "in the ledger, but no longer a gap (take it out of gl_function_table_gaps.inl): " << closed_names;
}

TEST_F(GLFunctionTables, TheGroupsOfTheFrontEndPolicyDoNotOverlap)
{
    for (const auto function : get_gl_functions() )
    {
        const int n_groups = (Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::bakes_current_matrix        (function) ? 1 : 0) +
                             (Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::changes_what_a_draw_writes  (function) ? 1 : 0) +
                             (Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::is_left_out_by_design       (function) ? 1 : 0);

        EXPECT_LE(n_groups,
                  1) << Capture::APIDumpNames::get_function_name(function);
    }
}

TEST_F(GLFunctionTables, TheChecksTellAFunctionThatNeitherPlaceKnowsFromOneThatBothDo)
{
    /* A query object is something that neither of them follows. */
    EXPECT_FALSE(does_front_end_follow     (APIInterceptor::APIFUNCTION_GL_GLBEGINQUERY) );
    EXPECT_FALSE(does_state_coalescing_know(APIInterceptor::APIFUNCTION_GL_GLBEGINQUERY) );

    EXPECT_TRUE(does_front_end_follow     (APIInterceptor::APIFUNCTION_GL_GLENABLE) );
    EXPECT_TRUE(does_state_coalescing_know(APIInterceptor::APIFUNCTION_GL_GLENABLE) );
}

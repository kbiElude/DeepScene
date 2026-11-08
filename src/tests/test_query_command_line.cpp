/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the command line that the query programs share (QueryCommandLine), the walker over the arguments, and the option that takes a number.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/query_command_line.h"
#include "utils/command_line/argument_walker.h"
#include "utils/command_line/number_parser.h"
#include <string>
#include <vector>

namespace
{
    typedef Analysis::APIDumpAnalysis::QueryCommandLine QueryCommandLine;

    /* The arguments of a program, as main gets them (the first is the name of the program). */
    struct Arguments
    {
        explicit Arguments(const std::vector<std::string>& in_argument_vec)
            :m_argument_vec(in_argument_vec)
        {
            m_argument_vec.insert(m_argument_vec.begin(),
                                  "program");

            for (auto& argument : m_argument_vec)
            {
                m_pointer_vec.push_back(&argument.at(0) );
            }
        }

        int get_argc() const
        {
            return static_cast<int>(m_pointer_vec.size() );
        }

        char** get_argv()
        {
            return m_pointer_vec.data();
        }

    private:
        std::vector<std::string> m_argument_vec;
        std::vector<char*>       m_pointer_vec;
    };

    struct ParseOutcome
    {
        std::string                error;
        std::vector<std::string>   positional_vec;
        Analysis::APIDumpAnalysis::QueryOptions options;
        bool                       result = false;
    };

    ParseOutcome parse(const std::vector<std::string>&                                       in_argument_vec,
                       const std::vector<Analysis::APIDumpAnalysis::QueryNumberOption>&       in_number_option_vec = {},
                       const QueryCommandLine::ProgramOptionFunc&                             in_program_option_func = QueryCommandLine::ProgramOptionFunc() )
    {
        Arguments    arguments(in_argument_vec);
        ParseOutcome outcome;

        const QueryCommandLine::PositionalFunc positional_func = [&](const std::string& in_argument,
                                                                     const size_t&      in_n_positional,
                                                                     std::string*       out_error_ptr)
        {
            (void) in_n_positional;
            (void) out_error_ptr;

            outcome.positional_vec.push_back(in_argument);

            return true;
        };

        outcome.result = QueryCommandLine::parse(arguments.get_argc(),
                                                 arguments.get_argv(),
                                                 in_number_option_vec,
                                                 in_program_option_func,
                                                 positional_func,
                                                &outcome.options,
                                                &outcome.error);

        return outcome;
    }
}

TEST(QueryCommandLine, TheOptionsThatEveryQueryProgramHasAreRead)
{
    const ParseOutcome outcome = parse({"file.passes", "--compact", "info", "--limit", "5", "--offset", "2", "--no-verify", "--workload", "w.workload"});

    ASSERT_TRUE(outcome.result) << outcome.error;

    EXPECT_TRUE(outcome.options.is_compact);
    EXPECT_TRUE(outcome.options.is_verify_off);
    EXPECT_EQ  (outcome.options.limit,
                5u);
    EXPECT_EQ  (outcome.options.offset,
                2u);
    EXPECT_EQ  (outcome.options.workload_filename,
                std::string("w.workload") );

    /* What is not an option is for the program, in order, and the values of the options are not among it. */
    ASSERT_EQ(outcome.positional_vec.size(),
              2u);
    EXPECT_EQ(outcome.positional_vec.at(0),
              std::string("file.passes") );
    EXPECT_EQ(outcome.positional_vec.at(1),
              std::string("info") );
}

TEST(QueryCommandLine, TheDefaultsAreThoseOfAListOf100FromTheStart)
{
    const ParseOutcome outcome = parse({"file.passes", "info"});

    ASSERT_TRUE(outcome.result) << outcome.error;

    EXPECT_FALSE(outcome.options.is_compact);
    EXPECT_FALSE(outcome.options.is_verify_off);
    EXPECT_EQ   (outcome.options.limit,
                 100u);
    EXPECT_EQ   (outcome.options.offset,
                 0u);
    EXPECT_TRUE (outcome.options.workload_filename.empty() );
}

TEST(QueryCommandLine, AnOptionThatTakesANumberIsGivenIt)
{
    uint64_t value = 0;

    const auto set_value_func = [&](const uint64_t& in_value)
    {
        value = in_value;
    };

    const ParseOutcome outcome = parse({"--call", "77", "file.textures"},
                                       {{"--call", set_value_func}});

    ASSERT_TRUE(outcome.result) << outcome.error;

    EXPECT_EQ(value,
              77u);
    ASSERT_EQ(outcome.positional_vec.size(),
              1u);
}

TEST(QueryCommandLine, TheProgramIsAskedAboutWhatItDoesNotKnow)
{
    std::string seen_value;

    const QueryCommandLine::ProgramOptionFunc program_option_func = [&](const std::string& in_argument,
                                                                        const bool&        in_has_next,
                                                                        const std::string& in_next_argument,
                                                                        bool*              out_is_known_ptr,
                                                                        bool*              out_uses_next_ptr,
                                                                        std::string*       out_error_ptr)
    {
        (void) out_error_ptr;

        *out_is_known_ptr  = (in_argument == "--class");
        *out_uses_next_ptr = *out_is_known_ptr;

        if (*out_is_known_ptr &&
            in_has_next)
        {
            seen_value = in_next_argument;
        }

        return true;
    };

    const ParseOutcome outcome = parse({"--class", "static", "file.deps"},
                                       {},
                                       program_option_func);

    ASSERT_TRUE(outcome.result) << outcome.error;

    EXPECT_EQ(seen_value,
              std::string("static") );
    ASSERT_EQ(outcome.positional_vec.size(),
              1u);
    EXPECT_EQ(outcome.positional_vec.at(0),
              std::string("file.deps") );
}

TEST(QueryCommandLine, AnOptionThatNobodyKnowsIsAnError)
{
    const ParseOutcome outcome = parse({"file.passes", "--bogus"});

    EXPECT_FALSE(outcome.result);
    EXPECT_EQ   (outcome.error,
                 std::string("Unknown option --bogus") );
}

TEST(QueryCommandLine, ANumberThatIsMissingOrIsNotOneIsAnErrorThatSaysWhich)
{
    const ParseOutcome missing = parse({"file.passes", "--limit"});
    const ParseOutcome wrong   = parse({"file.passes", "--offset", "x"});

    EXPECT_FALSE(missing.result);
    EXPECT_EQ   (missing.error,
                 std::string("--limit needs a number") );
    EXPECT_FALSE(wrong.result);
    EXPECT_EQ   (wrong.error,
                 std::string("--offset needs a number, not x") );
}

TEST(QueryCommandLine, AWorkloadOptionWithNoPathIsAnError)
{
    const ParseOutcome outcome = parse({"file.passes", "--workload"});

    EXPECT_FALSE(outcome.result);
    EXPECT_NE   (outcome.error.find("--workload needs the path"),
                 std::string::npos);
}

TEST(ArgumentWalker, AnArgumentThatTheFunctionTakesTheNextOneWithIsNotGivenOnItsOwn)
{
    Arguments arguments({"-o", "out.txt", "in.txt"});

    std::vector<std::string> seen_vec;
    std::string              error;

    const Utils::CommandLine::ArgumentWalker::HandlerFunc handler_func = [&](const Utils::CommandLine::ArgumentCursor& in_cursor,
                                                                             bool*                                     out_uses_next_ptr,
                                                                             std::string*                              out_error_ptr)
    {
        (void) out_error_ptr;

        seen_vec.push_back(in_cursor.get_argument() );

        *out_uses_next_ptr = (in_cursor.get_argument() == "-o");

        return true;
    };

    EXPECT_TRUE(Utils::CommandLine::ArgumentWalker::walk(arguments.get_argc(),
                                                         arguments.get_argv(),
                                                         handler_func,
                                                        &error) );

    ASSERT_EQ(seen_vec.size(),
              2u);
    EXPECT_EQ(seen_vec.at(0),
              std::string("-o") );
    EXPECT_EQ(seen_vec.at(1),
              std::string("in.txt") );
}

TEST(NumberParser, AnOptionThatTakesANumberSaysWhatIsWrongWithWhatItWasGiven)
{
    std::string error;
    uint64_t    value = 0;

    EXPECT_TRUE (Utils::CommandLine::NumberParser::parse_option_unsigned("--limit",
                                                                         true,
                                                                         "12",
                                                                         100,
                                                                        &value,
                                                                        &error) );
    EXPECT_EQ   (value,
                 12u);
    EXPECT_FALSE(Utils::CommandLine::NumberParser::parse_option_unsigned("--limit",
                                                                         true,
                                                                         "101",
                                                                         100,
                                                                        &value,
                                                                        &error) );
    EXPECT_EQ   (error,
                 std::string("--limit needs a number, not 101") );
    EXPECT_FALSE(Utils::CommandLine::NumberParser::parse_option_unsigned("--limit",
                                                                         false,
                                                                         "",
                                                                         100,
                                                                        &value,
                                                                        &error) );
    EXPECT_EQ   (error,
                 std::string("--limit needs a number") );
}

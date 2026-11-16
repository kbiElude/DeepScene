/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the breadcrumb that a replay leaves for a crash: the line that says which call is being made, and that the file has it without the
 * program having to close it.
 */
#include <gtest/gtest.h>
#include "temp_directory.h"
#include "tools/stereo_replay/inspector/command_line.h"
#include "tools/stereo_replay/inspector/crash_breadcrumb.h"
#include <fstream>
#include <iterator>

namespace
{
    typedef Stereo::Replay::Inspector::CrashBreadcrumb CrashBreadcrumb;

    /* glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pointer) as a call of thread 1. */
    Capture::APIDumpLoader::DumpedAPICall make_read_pixels_call()
    {
        Capture::APIDumpLoader::DumpedAPICall call;

        call.arg_in_vec =
        {
            APIInterceptor::APIFunctionArgument::create_i32       (0),
            APIInterceptor::APIFunctionArgument::create_i32       (0),
            APIInterceptor::APIFunctionArgument::create_i32       (1),
            APIInterceptor::APIFunctionArgument::create_i32       (1),
            APIInterceptor::APIFunctionArgument::create_u32_glenum(0x1907),
            APIInterceptor::APIFunctionArgument::create_u32_glenum(0x1401),
            APIInterceptor::APIFunctionArgument::create_void_ptr  (reinterpret_cast<void*>(static_cast<uintptr_t>(0x19fd98) ) )
        };

        call.func      = APIInterceptor::APIFUNCTION_GL_GLREADPIXELS;
        call.thread_id = 1;

        return call;
    }

    std::string read_file(const std::string& in_filename)
    {
        std::ifstream stream(in_filename,
                             std::ios::binary);

        return std::string(std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>() );
    }
}

TEST(CrashBreadcrumb, ACallIsALineWithItsFunctionAndItsArguments)
{
    const auto call = make_read_pixels_call();

    EXPECT_EQ(CrashBreadcrumb::format_call("begun",
                                           call,
                                           1036,
                                           12),
              "begun frame=1036 call=12 thread=1 glReadPixels(0, 0, 1, 1, 6407, 5121, 0x19fd98)");
}

TEST(CrashBreadcrumb, ADataChunkIsToldByItsNumber)
{
    Capture::APIDumpLoader::DumpedAPICall call;

    call.arg_in_vec = {APIInterceptor::APIFunctionArgument::create_data_chunk_id(77)};
    call.func       = APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF;

    EXPECT_NE(CrashBreadcrumb::format_call("begun",
                                           call,
                                           0,
                                           1).find("glLoadMatrixf(chunk#77)"),
              std::string::npos);
}

TEST(CrashBreadcrumb, TheFileHasTheCallThatWasBegunWithoutBeingClosed)
{
    Tests::TempDirectory directory;
    const std::string    filename = directory.get_path() + "\\breadcrumb.txt";
    const auto           call     = make_read_pixels_call();

    {
        CrashBreadcrumb breadcrumb(filename);

        ASSERT_TRUE(breadcrumb.is_open() );

        EXPECT_TRUE(breadcrumb.should_execute_call(call,
                                                   5) );

        /* A reader of the file sees the line while the replay is still on the call: this is what a crash leaves. */
        EXPECT_EQ(read_file(filename).find("begun frame=5 call=1 thread=1 glReadPixels("),
                  0u);
    }
}

TEST(CrashBreadcrumb, TheLineSaysWhenTheCallIsFinished)
{
    Tests::TempDirectory directory;
    const std::string    filename = directory.get_path() + "\\breadcrumb.txt";
    const auto           call     = make_read_pixels_call();
    CrashBreadcrumb      breadcrumb(filename);

    breadcrumb.should_execute_call(call,
                                   5);
    breadcrumb.on_call_replayed   (call,
                                   5,
                                   true);

    EXPECT_EQ(read_file(filename).find("finished frame=5 call=1 "),
              0u);
}

TEST(CrashBreadcrumb, TheCallsAreCounted)
{
    Tests::TempDirectory directory;
    const std::string    filename = directory.get_path() + "\\breadcrumb.txt";
    const auto           call     = make_read_pixels_call();
    CrashBreadcrumb      breadcrumb(filename);

    breadcrumb.should_execute_call(call,
                                   5);
    breadcrumb.should_execute_call(call,
                                   5);
    breadcrumb.should_execute_call(call,
                                   6);

    EXPECT_EQ(read_file(filename).find("begun frame=6 call=3 "),
              0u);
}

TEST(CrashBreadcrumb, WhatIsLeftOfALongerLineIsGone)
{
    Tests::TempDirectory directory;
    const std::string    filename = directory.get_path() + "\\breadcrumb.txt";
    const auto           long_call = make_read_pixels_call();
    CrashBreadcrumb      breadcrumb(filename);

    Capture::APIDumpLoader::DumpedAPICall short_call;

    short_call.func = APIInterceptor::APIFUNCTION_GL_GLEND;

    breadcrumb.should_execute_call(long_call,
                                   5);
    breadcrumb.should_execute_call(short_call,
                                   5);

    const std::string text     = read_file(filename);
    const size_t      line_end = text.find('\n');

    ASSERT_NE(line_end,
              std::string::npos);

    EXPECT_NE(text.substr(0,
                          line_end).find("glEnd()"),
              std::string::npos);

    for (size_t n_byte = line_end + 1;
                n_byte < text.size();
              ++n_byte)
    {
        EXPECT_EQ(text.at(n_byte),
                  '\0') << "byte " << n_byte;
    }
}

TEST(CrashBreadcrumb, AFileThatCannotBeMadeIsNoHarm)
{
    const auto      call = make_read_pixels_call();
    CrashBreadcrumb breadcrumb("");

    EXPECT_FALSE(breadcrumb.is_open() );
    EXPECT_TRUE (breadcrumb.should_execute_call(call,
                                                0) );

    breadcrumb.on_call_replayed(call,
                                0,
                                true);
}

TEST(InspectorCommandLineTest, TheBreadcrumbFileIsRead)
{
    std::string                                 error;
    Stereo::Replay::Inspector::Options options;

    ASSERT_TRUE(Stereo::Replay::Inspector::CommandLine::parse({"a.workload", "--breadcrumb", "last_call.txt"},
                                                              &options,
                                                              &error) ) << error;

    EXPECT_EQ(options.breadcrumb_filename,
              "last_call.txt");
}

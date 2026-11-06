/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Where the data files of the tests are.
 */
#ifndef TESTS_DATA_FIXTURES_H
#define TESTS_DATA_FIXTURES_H

#include "Analysis/APIDumpAnalysis/model.h"
#include <string>

namespace Tests
{
    class DataFixtures
    {
    public:
        /* The analysis of the slice below, made once for all the tests: copy it to change it. */
        static const Analysis::APIDumpAnalysis::AnalysisResult& get_fr08_slice_analysis();

        /* Frames 0-3 (the setup of fr08), 100, 101, 14364, 14365, 219679 and 219680 of a capture of fr08 (see
         * data/README.txt): one pass; three passes; a scene rendered to a texture and post-processed. The data chunks
         * bigger than 1 KB are empty.
         */
        static std::string get_fr08_slice_filename();

        /* A recipe which uses every field (data/recipes/fr08_example.json). */
        static std::string get_recipe_example_filename();

        /* The folder of this project, with src, cmake and apps in it. */
        static std::string get_repository_root_path();

        /* The path of the program which runs the tests: a file which is there, and is not what a test asks a loader for. */
        static std::string get_test_executable_path();

        /* A name for a session (see FrameChannel) that no other test, and no other run of the tests, has. */
        static std::string make_session_id(const std::string& in_name);
    };
}

#endif /* TESTS_DATA_FIXTURES_H */

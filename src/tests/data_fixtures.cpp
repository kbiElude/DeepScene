/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "data_fixtures.h"
#include <Windows.h>
#include "analysis/api_dump_analysis/frontends/analysis_session.h"

const Analysis::APIDumpAnalysis::AnalysisResult& Tests::DataFixtures::get_fr08_slice_analysis()
{
    static const Analysis::APIDumpAnalysis::AnalysisResult analysis = []
    {
        Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
        Analysis::APIDumpAnalysis::AnalysisResult                   result;

        session.run(get_fr08_slice_filename().c_str(),
                    nullptr,
                    nullptr);

        result.model              = session.get_model();
        result.summary            = session.get_summary();
        result.workload_reference = session.get_workload_reference();

        return result;
    }();

    return analysis;
}

std::string Tests::DataFixtures::get_fr08_slice_filename()
{
    return std::string(DEMOVRPLAYER_TESTS_DATA_DIR) + "/fr08_slice.workload";
}

std::string Tests::DataFixtures::get_recipe_example_filename()
{
    return std::string(DEMOVRPLAYER_TESTS_DATA_DIR) + "/recipes/fr08_example.json";
}

std::string Tests::DataFixtures::get_repository_root_path()
{
    return DEMOVRPLAYER_REPOSITORY_ROOT;
}

std::string Tests::DataFixtures::get_test_executable_path()
{
    char path_i8vec260[MAX_PATH] = {};

    ::GetModuleFileNameA(nullptr,
                         path_i8vec260,
                         MAX_PATH);

    return path_i8vec260;
}

std::string Tests::DataFixtures::make_session_id(const std::string& in_name)
{
    return "test-" + in_name + "-" + std::to_string(::GetCurrentProcessId() );
}

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks that the source tree is the release: a program which cmake/Install.cmake installs has a folder where it is installed, named after it
 * (src/tools/analysis/recipe_bake for tools/analysis/RecipeBake.exe), and each folder of a program is installed. A folder is the name of the
 * program in snake_case, without the prefix of its group (runtime/launcher for StereoLauncher, tools/stereo_replay/player for
 * StereoReplayPlayer). What is installed from the interceptor (the dumper and its printer) is in its own tree.
 */
#include <gtest/gtest.h>
#include "data_fixtures.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace
{
    /* The folder of the source of the VR player: it is in the folder of the release, and not in a group. */
    constexpr const char* const VR_PLAYER_FOLDER = "vr_player";

    typedef std::map<std::string, std::string> DestinationMap; /* the name of an installed program, and the folder it is installed in */

    /* The part of the name of a program that its group says: the folder of the program does not repeat it. */
    std::string get_group_prefix(const std::string& in_destination)
    {
        const std::map<std::string, std::string> prefix_map = {{"runtime",             "Stereo"},
                                                               {"tools/stereo_replay", "StereoReplay"}};
        const auto                               found      = prefix_map.find(in_destination);
        std::string                              result;

        if (found != prefix_map.end() )
        {
            result = found->second;
        }

        return result;
    }

    /* StereoReplayEngineDLL, installed in tools/stereo_replay, is the folder engine_dll; VRPlayer is vr_player. */
    std::string get_folder_name(const std::string& in_program_name,
                                const std::string& in_destination)
    {
        const auto        is_lower_func = [](const char in_character)
        {
            return std::islower(static_cast<unsigned char>(in_character) ) != 0;
        };
        const auto        is_upper_func = [](const char in_character)
        {
            return std::isupper(static_cast<unsigned char>(in_character) ) != 0;
        };
        const std::string prefix = get_group_prefix(in_destination);
        std::string       name   = in_program_name;
        std::string       result;

        if (name.compare(0,
                         prefix.size(),
                         prefix) == 0)
        {
            name = name.substr(prefix.size() );
        }

        for (size_t n_character = 0;
                    n_character < name.size();
                  ++n_character)
        {
            const char character         = name.at(n_character);
            const bool is_next_lower     = (n_character + 1 < name.size() &&
                                            is_lower_func(name.at(n_character + 1) ) );
            const bool is_previous_lower = (n_character > 0               &&
                                            is_lower_func(name.at(n_character - 1) ) );
            const bool is_previous_upper = (n_character > 0               &&
                                            is_upper_func(name.at(n_character - 1) ) );

            /* A word starts at a capital after a small letter (RecipeBake), and at the last capital of a run which is followed by a small letter (APIDump). */
            if (is_upper_func(character)  == true &&
                (is_previous_lower        == true ||
                 (is_previous_upper       == true &&
                  is_next_lower           == true) ) )
            {
                result += '_';
            }

            result += static_cast<char>(std::tolower(static_cast<unsigned char>(character) ) );
        }

        return result;
    }

    std::filesystem::path get_program_folder(const std::string& in_name,
                                             const std::string& in_destination)
    {
        const std::filesystem::path src_path = std::filesystem::path(Tests::DataFixtures::get_repository_root_path() ) / "src";
        const std::string           folder   = get_folder_name(in_name,
                                                               in_destination);

        return (in_destination == ".") ? src_path / folder
                                       : src_path / in_destination / folder;
    }

    /* The programs that cmake/Install.cmake installs from this project. */
    DestinationMap read_installed_programs()
    {
        const std::set<std::string> interceptor_name_set = {"APIDumper", "APIDumperDLL", "APIDumpPrinter"};
        std::ifstream file(std::filesystem::path(Tests::DataFixtures::get_repository_root_path() ) / "cmake" / "Install.cmake");
        std::stringstream text;
        DestinationMap    result;

        text << file.rdbuf();

        const std::string text_string = text.str();
        const std::regex  line_regex  (R"regex(install_(?:with_suffix|as_built)\(\s*(\w+)\s+"([^"]+)"\s*\))regex");

        for (std::sregex_iterator match(text_string.begin(), text_string.end(), line_regex), end; match != end; ++match)
        {
            if (interceptor_name_set.count((*match)[1].str() ) == 0)
            {
                result[(*match)[1].str()] = (*match)[2].str();
            }
        }

        return result;
    }
}

TEST(Layout, AProgramWhichIsInstalledHasTheFolderOfItsNameWhereItIsInstalled)
{
    const DestinationMap installed_map = read_installed_programs();

    ASSERT_FALSE(installed_map.empty() );

    for (const auto& program : installed_map)
    {
        const std::filesystem::path folder = get_program_folder(program.first,
                                                                program.second);

        EXPECT_TRUE(std::filesystem::exists(folder / "CMakeLists.txt") ) << program.first << " is installed in " << program.second << ", and there is no " << folder.string() << " with its CMakeLists.txt";
    }
}

TEST(Layout, AFolderOfAProgramIsInstalled)
{
    std::set<std::filesystem::path> expected_set;
    std::set<std::filesystem::path> folder_set;
    const DestinationMap            installed_map = read_installed_programs();
    const std::filesystem::path     src_path      = std::filesystem::path(Tests::DataFixtures::get_repository_root_path() ) / "src";

    for (const auto& program : installed_map)
    {
        expected_set.insert(get_program_folder(program.first,
                                               program.second) );
    }

    /* The programs are in src/runtime, src/tools/<group> and src/vr_player. */
    for (const char* group_ptr : {"runtime", "tools/analysis", "tools/stereo_replay"})
    {
        for (const auto& entry : std::filesystem::directory_iterator(src_path / group_ptr) )
        {
            if (entry.is_directory() )
            {
                folder_set.insert(entry.path() );
            }
        }
    }

    folder_set.insert(src_path / VR_PLAYER_FOLDER);

    for (const auto& folder : folder_set)
    {
        EXPECT_EQ(expected_set.count(folder),
                  1u) << folder.string() << " is a folder of a program, and cmake/Install.cmake does not install a program of that name where the folder is";
    }
}

TEST(Layout, UtilsIncludesNothingOfTheOtherLibrariesOrOfAProgram)
{
    const std::regex            include_regex  (R"regex(^\s*#\s*include\s+"([A-Za-z0-9_]+)/)regex");
    size_t                      n_files        = 0;
    const std::set<std::string> other_name_set = {"analysis", "headset", "runtime", "stereo", "tests", "tools", "vr_player"};
    std::string                 report;
    const std::filesystem::path utils_path     = std::filesystem::path(Tests::DataFixtures::get_repository_root_path() ) / "src" / "libs" / "utils";

    for (const auto& entry : std::filesystem::recursive_directory_iterator(utils_path) )
    {
        if (entry.is_regular_file() == false)
        {
            continue;
        }

        std::ifstream file(entry.path());
        std::string   line;

        n_files++;

        while (std::getline(file,
                            line) )
        {
            std::smatch match;

            if (std::regex_search(line,
                                  match,
                                  include_regex) )
            {
                if (other_name_set.count(match[1].str() ) != 0)
                {
                    report += entry.path().string() + ": " + line + "\n";
                }
            }
        }
    }

    ASSERT_GT(n_files,
              0u);
    EXPECT_TRUE(report.empty() ) << "Utils is used by the other libraries, and uses none of them:\n" << report;
}

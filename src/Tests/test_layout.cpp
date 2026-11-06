/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks that the source tree is the release: a program which cmake/Install.cmake installs has a folder of the same name where it is installed
 * (src/tools/Analysis/RecipeBake for tools/Analysis/RecipeBake.exe), and each folder of a program is installed. What is installed from the
 * interceptor (the dumper and its printer) is in its own tree.
 */
#include <gtest/gtest.h>
#include "data_fixtures.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace
{
    /* Where the programs of the folders which are named after them are: the VR player is in the folder of the source, since it is in the folder of the release. */
    constexpr const char* const VR_PLAYER_NAME = "VRPlayer";

    typedef std::map<std::string, std::string> DestinationMap; /* the name of an installed program, and the folder it is installed in */

    /* The programs that cmake/Install.cmake installs from this project. */
    DestinationMap read_installed_programs()
    {
        const std::set<std::string> interceptor_name_set = {"APIDumper", "APIDumperDLL", "APIDumpPrinter"};
        std::ifstream               file(std::filesystem::path(Tests::DataFixtures::get_repository_root_path() ) / "cmake" / "Install.cmake");
        std::stringstream           text;
        DestinationMap              result;

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

    std::filesystem::path get_program_folder(const std::string& in_name,
                                             const std::string& in_destination)
    {
        const std::filesystem::path src_path = std::filesystem::path(Tests::DataFixtures::get_repository_root_path() ) / "src";

        return (in_destination == ".") ? src_path / in_name
                                       : src_path / in_destination / in_name;
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
    const DestinationMap                installed_map = read_installed_programs();
    const std::filesystem::path         src_path      = std::filesystem::path(Tests::DataFixtures::get_repository_root_path() ) / "src";
    std::set<std::filesystem::path>     folder_set;

    /* The programs are in src/runtime, src/tools/<group> and src/VRPlayer. */
    for (const char* group_ptr : {"runtime", "tools/Analysis", "tools/Replay"})
    {
        for (const auto& entry : std::filesystem::directory_iterator(src_path / group_ptr) )
        {
            if (entry.is_directory() )
            {
                folder_set.insert(entry.path() );
            }
        }
    }

    folder_set.insert(src_path / VR_PLAYER_NAME);

    for (const auto& folder : folder_set)
    {
        const std::string name  = folder.filename().string();
        const auto        found = installed_map.find(name);

        ASSERT_NE(found,
                  installed_map.end() ) << name << " has a folder in the source and is not installed by cmake/Install.cmake";

        const std::filesystem::path expected_folder = get_program_folder(name,
                                                                         found->second);

        EXPECT_EQ(expected_folder,
                  folder) << name << " is installed in " << found->second << ", which is not where its folder is";
    }
}

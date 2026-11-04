/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks that the code which is written by hand does not repeat itself: a run of eight lines (not counting blank lines, braces, comments and
 * includes) which is the same in two files is a piece of logic that is written twice. The duplicates that are known are listed in
 * data/known_duplicates.txt, with how many runs each group of files shares; a new one, or one that has grown, fails the test. A duplicate which is
 * removed makes the list shorter: take it out of the file.
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    constexpr size_t N_LINES_IN_RUN    = 8;
    constexpr size_t N_MIN_RUN_BYTES   = 250;

    typedef std::map<std::string, size_t> GroupCountMap; /* the files of a group, joined with '|', and the runs they share */

    std::filesystem::path get_root_path()
    {
        /* The data folder is src/Tests/data of this project. */
        return std::filesystem::path(DEMOVRPLAYER_TESTS_DATA_DIR).parent_path().parent_path().parent_path();
    }

    /* Generated, third party and test files are not what this checks. */
    bool is_checked_file(const std::string& in_relative_path)
    {
        const bool is_source = (in_relative_path.size() > 2 &&
                                (in_relative_path.compare(in_relative_path.size() - 2, 2, ".h")   == 0 ||
                                 (in_relative_path.size() > 4 && in_relative_path.compare(in_relative_path.size() - 4, 4, ".cpp") == 0) ) );

        return (is_source                                                        &&
                in_relative_path.find("/Tests/")              == std::string::npos &&
                in_relative_path.find("/entrypoints/")        == std::string::npos &&
                in_relative_path.find("Hooks/OpenGL/")        == std::string::npos);
    }

    std::vector<std::string> get_normalized_lines(const std::filesystem::path& in_path)
    {
        std::ifstream            file(in_path);
        std::string              line;
        std::vector<std::string> result;

        while (std::getline(file,
                            line) )
        {
            std::string normalized;
            bool        was_space = true;

            for (const char character : line)
            {
                const bool is_space = (character == ' ' || character == '\t' || character == '\r');

                if (is_space == false)
                {
                    normalized += character;
                }
                else
                if (was_space == false)
                {
                    normalized += ' ';
                }

                was_space = is_space;
            }

            while (normalized.empty() == false &&
                   normalized.back()  == ' ')
            {
                normalized.pop_back();
            }

            const bool is_noise = (normalized.empty()                  ||
                                   normalized == "{"                   ||
                                   normalized == "}"                   ||
                                   normalized.compare(0, 2, "//") == 0 ||
                                   normalized.compare(0, 2, "/*") == 0 ||
                                   normalized[0] == '*'                ||
                                   normalized[0] == '#');

            if (is_noise == false)
            {
                result.push_back(normalized);
            }
        }

        return result;
    }

    GroupCountMap find_duplicate_groups()
    {
        const auto                                           root = get_root_path();
        std::map<std::string, std::set<std::string> >        files_by_run;
        std::map<std::string, std::vector<std::string> >     file_vec_by_run;
        GroupCountMap                                        result;
        const std::vector<std::filesystem::path>             folder_vec = {root / "src",
                                                                           root / "include",
                                                                           root / "deps" / "APIInterceptor" / "src",
                                                                           root / "deps" / "APIInterceptor" / "include"};

        for (const auto& folder : folder_vec)
        {
            if (std::filesystem::exists(folder) == false)
            {
                continue;
            }

            for (const auto& entry : std::filesystem::recursive_directory_iterator(folder) )
            {
                if (entry.is_regular_file() == false)
                {
                    continue;
                }

                std::string relative_path = std::filesystem::relative(entry.path(),
                                                                      root).generic_string();

                if (is_checked_file("/" + relative_path) == false)
                {
                    continue;
                }

                const auto line_vec = get_normalized_lines(entry.path() );

                for (size_t n_line = 0;
                            n_line + N_LINES_IN_RUN <= line_vec.size();
                          ++n_line)
                {
                    std::string run;

                    for (size_t n_run_line = 0;
                                n_run_line < N_LINES_IN_RUN;
                              ++n_run_line)
                    {
                        run += line_vec.at(n_line + n_run_line) + "\n";
                    }

                    if (run.size() >= N_MIN_RUN_BYTES)
                    {
                        files_by_run[run].insert(relative_path);
                    }
                }
            }
        }

        for (const auto& run : files_by_run)
        {
            if (run.second.size() > 1)
            {
                std::string group;

                for (const auto& file : run.second)
                {
                    group += (group.empty() ? "" : "|") + file;
                }

                result[group]++;
            }
        }

        return result;
    }

    GroupCountMap read_known_groups()
    {
        std::ifstream file(std::string(DEMOVRPLAYER_TESTS_DATA_DIR) + "/known_duplicates.txt");
        std::string   line;
        GroupCountMap result;

        while (std::getline(file,
                            line) )
        {
            const auto separator = line.find(' ');

            if (separator != std::string::npos &&
                line[0]   != '#')
            {
                std::string group = line.substr(separator + 1);

                while (group.empty() == false &&
                       (group.back() == '\r' || group.back() == ' ') )
                {
                    group.pop_back();
                }

                result[group] = static_cast<size_t>(std::stoul(line.substr(0,
                                                                           separator) ) );
            }
        }

        return result;
    }
}

TEST(DuplicateCode, NothingNewIsWrittenTwice)
{
    const auto    known_map = read_known_groups();
    const auto    found_map = find_duplicate_groups();
    std::string   report;

    for (const auto& group : found_map)
    {
        const auto known_iterator = known_map.find(group.first);

        if (known_iterator == known_map.end() )
        {
            report += "new: " + std::to_string(group.second) + " " + group.first + "\n";
        }
        else
        if (group.second > known_iterator->second)
        {
            report += "grown from " + std::to_string(known_iterator->second) + ": " + std::to_string(group.second) + " " + group.first + "\n";
        }
    }

    EXPECT_TRUE(report.empty() ) << "Code which is the same in more than one file (put it in one place, or, if it must stay, in data/known_duplicates.txt):\n" << report;
}

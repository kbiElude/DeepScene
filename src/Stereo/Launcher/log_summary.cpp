/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/Launcher/log_summary.h"
#include <stdlib.h>

namespace
{
    /* The number that comes right before the text in the line, 0 if the text or a number before it is not there. */
    uint64_t get_number_before(const std::string& in_line,
                               const char*        in_text_ptr)
    {
        const size_t text_position = in_line.find(in_text_ptr);
        uint64_t     result        = 0;
        size_t       n_digit_end   = 0;
        size_t       n_digit_start = 0;

        if (text_position == std::string::npos)
        {
            goto end;
        }

        n_digit_end   = text_position;
        n_digit_start = n_digit_end;

        while (n_digit_start > 0                  &&
               in_line.at(n_digit_start - 1) >= '0' &&
               in_line.at(n_digit_start - 1) <= '9')
        {
            n_digit_start--;
        }

        if (n_digit_start != n_digit_end)
        {
            result = ::strtoull(in_line.substr(n_digit_start,
                                               n_digit_end - n_digit_start).c_str(),
                                nullptr,
                                10);
        }

end:
        return result;
    }
}

std::vector<std::string> Stereo::Launcher::LogSummarizer::make_report_lines(const LogSummary& in_summary)
{
    std::vector<std::string> result_vec;

    if (in_summary.has_statistics == false)
    {
        result_vec.push_back("[*] The log has no statistics of the converter: the application ran for fewer frames than it writes them after, or it was not converted.");

        goto end;
    }

    result_vec.push_back("[*] The converter saw " + std::to_string(in_summary.n_frames) + " frames: " + std::to_string(in_summary.n_affected_draws) + " draws were affected, " +
                         std::to_string(in_summary.n_unaffected_draws) + " unaffected and " + std::to_string(in_summary.n_skipped_draws) + " skipped.");

    if (in_summary.n_unknown_draws != 0)
    {
        result_vec.push_back("[!] " + std::to_string(in_summary.n_unknown_draws) + " draws are of a signature that the recipe does not know (is the recipe of this application and this capture? "
                             "RecipeBake --check says what it does not know).");
    }

    if (in_summary.n_unknown_frames != 0)
    {
        result_vec.push_back("[!] " + std::to_string(in_summary.n_unknown_frames) + " frames are of a scene that the recipe has no distance for (see the unknown frame policy of the recipe).");
    }

    if (in_summary.n_failed_twins != 0)
    {
        result_vec.push_back("[!] " + std::to_string(in_summary.n_failed_twins) + " surfaces could not be given a twin for the right eye.");
    }

    if (in_summary.n_unmirrored_calls != 0)
    {
        result_vec.push_back("[!] " + std::to_string(in_summary.n_unmirrored_calls) + " calls were not made for the right eye (the log names the first of them).");
    }

end:
    return result_vec;
}

Stereo::Launcher::LogSummary Stereo::Launcher::LogSummarizer::summarize(const std::vector<std::string>& in_log_line_vec)
{
    const std::string marker = "After ";
    LogSummary        result;

    for (const auto& line : in_log_line_vec)
    {
        const size_t marker_position = line.find(marker);

        if (marker_position == std::string::npos ||
            line.find(" frames: ") == std::string::npos)
        {
            continue;
        }

        const std::string statistics = line.substr(marker_position);

        result.has_statistics     = true;
        result.n_affected_draws   = get_number_before(statistics, " affected draws");
        result.n_failed_twins     = get_number_before(statistics, " twins that could not be made");
        result.n_frames           = get_number_before(statistics, " frames: ");
        result.n_skipped_draws    = get_number_before(statistics, " skipped");
        result.n_unaffected_draws = get_number_before(statistics, " unaffected");
        result.n_unknown_draws    = get_number_before(statistics, " of a signature the recipe does not know");
        result.n_unknown_frames   = get_number_before(statistics, " frames of a scene it has no distance for");
        result.n_unmirrored_calls = get_number_before(statistics, " calls not made for the right eye");
    }

    return result;
}

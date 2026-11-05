/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for OverlayProbe: which of the candidates for 2D overlays make pixels of the picture, with the frame replayed with and without each of them.
 */
#include <gtest/gtest.h>
#include "Stereo/Replay/ConsistencyChecker/overlay_probe.h"
#include "temp_directory.h"
#include <fstream>
#include <iterator>
#include <string>

namespace
{
    constexpr const char* HIDDEN_TEXT = "0x0000000000002222";
    constexpr uint64_t    HUD_KEY     = 0x1111;
    constexpr const char* HUD_TEXT    = "0x0000000000001111";

    std::string read_text_file(const std::string& in_path)
    {
        std::ifstream stream(in_path,
                             std::ios::binary);

        return std::string(std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>() );
    }

    void write_text_file(const std::string& in_path,
                         const std::string& in_text)
    {
        std::ofstream stream(in_path,
                             std::ios::binary);

        stream << in_text;
    }

    std::string make_candidate(const char* in_structure_key_ptr,
                               const bool& in_has_frame)
    {
        return std::string("{\"signature_key\":\"0x00000000000000aa\",\"structure_key\":\"") + in_structure_key_ptr + "\",\"pass_id\":1,\"n_frames\":3," +
               ((in_has_frame) ? "\"first_frame_index\":7," : "") +
               "\"agreement\":\"both\",\"replay_class\":\"unaffected\",\"live_class\":\"unaffected\",\"reasons\":[\"static_camera\"]}";
    }

    /* A picture of 4 by 2 pixels, which is dark where nothing is drawn, and which has the HUD in its first pixel if the recipe does not skip it. The hidden draw makes
     * nothing that can be seen.
     */
    bool render(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                const uint32_t&                                    in_frame_index,
                Stereo::Replay::FrameImages::Image*                out_image_ptr,
                std::string*                                       out_error_ptr)
    {
        bool is_hud_skipped = false;

        for (const auto key : in_options.recipe.skipped.draw_key_vec)
        {
            is_hud_skipped = (is_hud_skipped || key == HUD_KEY);
        }

        out_image_ptr->height       = 2;
        out_image_ptr->width        = 4;
        out_image_ptr->pixel_u8_vec = std::vector<uint8_t>(4 * 2 * 4,
                                                           100);

        if (is_hud_skipped == false)
        {
            out_image_ptr->pixel_u8_vec.at(0) = 250;
        }

        (void) in_frame_index;
        (void) out_error_ptr;

        return true;
    }
}

TEST(OverlayProbe, ADrawWhichMakesPixelsIsWrittenWithAnImageAndOneWhichMakesNoneIsNot)
{
    Tests::TempDirectory                        directory;
    Stereo::Replay::ConsistencyChecker::Options options;
    std::string                                 error;

    write_text_file(directory.get_path() + "\\candidates_in.json",
                    "{\"candidates\":[" + make_candidate(HUD_TEXT,    true)  + "," +
                                         make_candidate(HIDDEN_TEXT, true)  + "," +
                                         make_candidate(HUD_TEXT,    false) + "]}");

    options.dump_directory         = directory.get_path() + "\\out";
    options.overlay_probe_filename = directory.get_path() + "\\candidates_in.json";

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::OverlayProbe::run(options,
                                                                      render,
                                                                     &error) ) << error;

    const std::string list = read_text_file(options.dump_directory + "\\candidates.json");
    const std::string page = read_text_file(options.dump_directory + "\\index.html");

    /* The candidate of no frame is not probed. */
    EXPECT_NE(list.find("\"n_changed_pixels\": 1"), std::string::npos) << list;
    EXPECT_NE(list.find("\"n_changed_pixels\": 0"), std::string::npos) << list;
    EXPECT_NE(list.find("candidate_000_frame_7.png"), std::string::npos) << list;
    EXPECT_EQ(list.find("candidate_001"), std::string::npos) << list;
    EXPECT_EQ(list.find("candidate_002"), std::string::npos) << list;
    EXPECT_FALSE(read_text_file(options.dump_directory + "\\candidate_000_frame_7.png").empty() );
    EXPECT_TRUE (read_text_file(options.dump_directory + "\\candidate_001_frame_7.png").empty() );
    EXPECT_NE(page.find("candidate_000_frame_7.png"), std::string::npos);
}

TEST(OverlayProbe, AListThatIsNotJsonFails)
{
    Tests::TempDirectory                        directory;
    Stereo::Replay::ConsistencyChecker::Options options;
    std::string                                 error;

    write_text_file(directory.get_path() + "\\bad.json",
                    "{\"candidates\":");

    options.dump_directory         = directory.get_path() + "\\out";
    options.overlay_probe_filename = directory.get_path() + "\\bad.json";

    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::OverlayProbe::run(options,
                                                                       render,
                                                                      &error) );
    EXPECT_FALSE(error.empty() );
}

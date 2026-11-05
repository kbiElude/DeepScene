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
    constexpr uint64_t    SCENE_KEY   = 0x3333;
    constexpr const char* SCENE_TEXT  = "0x0000000000003333";

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

    /* How deep the vertices of the pass of a candidate are: at one depth, or from 30 to 100. */
    std::string make_depth_text(const bool& in_is_flat)
    {
        return (in_is_flat) ? "\"depth\":{\"has_depth\":true,\"n_vertices\":8,\"near\":5,\"median\":5,\"far\":5,\"far_to_near_ratio\":1,\"is_flat\":true},"
                            : "\"depth\":{\"has_depth\":true,\"n_vertices\":8,\"near\":30,\"median\":60,\"far\":100,\"far_to_near_ratio\":3.3,\"is_flat\":false},";
    }

    std::string make_candidate(const char*        in_structure_key_ptr,
                               const bool&         in_has_frame,
                               const std::string&  in_depth_text = std::string() )
    {
        return std::string("{\"signature_key\":\"0x00000000000000aa\",\"structure_key\":\"") + in_structure_key_ptr + "\",\"pass_id\":1,\"n_frames\":3," +
               ((in_has_frame) ? "\"first_frame_index\":7," : "") + in_depth_text +
               "\"agreement\":\"both\",\"replay_class\":\"unaffected\",\"live_class\":\"unaffected\",\"reasons\":[\"static_camera\"]}";
    }

    /* A picture of 4 by 2 pixels, which is dark where nothing is drawn, and which has the HUD in its first pixel if the recipe does not skip it, and the scene in six
     * of them. The hidden draw makes nothing that can be seen.
     */
    bool render(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                const uint32_t&                                    in_frame_index,
                Stereo::Replay::FrameImages::Image*                out_image_ptr,
                std::string*                                       out_error_ptr)
    {
        bool is_hud_skipped   = false;
        bool is_scene_skipped = false;

        for (const auto key : in_options.recipe.skipped.draw_key_vec)
        {
            is_hud_skipped   = (is_hud_skipped   || key == HUD_KEY);
            is_scene_skipped = (is_scene_skipped || key == SCENE_KEY);
        }

        out_image_ptr->height       = 2;
        out_image_ptr->width        = 4;
        out_image_ptr->pixel_u8_vec = std::vector<uint8_t>(4 * 2 * 4,
                                                           100);

        if (is_hud_skipped == false)
        {
            out_image_ptr->pixel_u8_vec.at(0) = 250;
        }

        /* The scene makes 6 of the 8 pixels. */
        if (is_scene_skipped == false)
        {
            for (size_t n_pixel = 1;
                        n_pixel < 7;
                      ++n_pixel)
            {
                out_image_ptr->pixel_u8_vec.at(n_pixel * 4) = 200;
            }
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

TEST(OverlayProbe, ADrawThatTheRecipeLeavesAloneIsToBeLookedAtTwiceIfItsPassIsDeepOrItMakesMuchOfThePicture)
{
    Tests::TempDirectory                        directory;
    Stereo::Replay::ConsistencyChecker::Options options;
    std::string                                 error;

    /* A flat HUD, a scene that is deep and makes most of the picture, and a flat draw that makes most of it (a fade): the last is for a person to see too. */
    write_text_file(directory.get_path() + "\\candidates_in.json",
                    "{\"candidates\":[" + make_candidate(HUD_TEXT,   true, make_depth_text(true) )  + "," +
                                         make_candidate(SCENE_TEXT, true, make_depth_text(false) ) + "]}");

    options.dump_directory         = directory.get_path() + "\\out";
    options.overlay_probe_filename = directory.get_path() + "\\candidates_in.json";

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::OverlayProbe::run(options,
                                                                      render,
                                                                     &error) ) << error;

    const std::string list = read_text_file(options.dump_directory + "\\candidates.json");
    const std::string page = read_text_file(options.dump_directory + "\\index.html");

    /* The HUD is flat and makes a part of the picture: nothing to say. The scene is deep, and 75% of the picture. */
    EXPECT_NE(list.find("\"deep_depth\""),               std::string::npos) << list;
    EXPECT_NE(list.find("\"covers_much_of_picture\""),   std::string::npos) << list;
    EXPECT_NE(list.find("\"far_to_near_ratio\": 3.3"),   std::string::npos) << list;
    EXPECT_NE(list.find("\"n_picture_pixels\": 8"),      std::string::npos) << list;
    EXPECT_NE(page.find("class=\"badge\">deep_depth"),   std::string::npos);
    EXPECT_NE(page.find("makes 75.0% of the picture"),   std::string::npos);
    EXPECT_NE(page.find("data-suspect=\"1\""),           std::string::npos);
    EXPECT_NE(page.find("data-suspect=\"0\""),           std::string::npos);
}

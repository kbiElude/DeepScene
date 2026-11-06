/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for OverlayProbe: which of the candidates for 2D overlays make pixels of the picture, with the frame replayed with and without each of them.
 */
#include <gtest/gtest.h>
#include "tools/stereo_replay/consistency_checker/overlay_probe.h"
#include "temp_directory.h"
#include <fstream>
#include <iterator>
#include <string>

namespace
{
    constexpr const char* HIDDEN_TEXT     = "0x0000000000002222";
    constexpr uint64_t    HUD_KEY         = 0x1111;
    constexpr const char* HUD_TEXT        = "0x0000000000001111";
    constexpr uint64_t    MAIN_PASS_KEY   = 0x99;
    constexpr const char* MAIN_PASS_TEXT  = "0x0000000000000099";
    constexpr const char* OTHER_PASS_TEXT = "0x0000000000000077";
    constexpr uint64_t    SCENE_KEY       = 0x3333;
    constexpr const char* SCENE_TEXT      = "0x0000000000003333";

    /* No function that runs the parts of the probe: it is done in the process. */
    const Stereo::Replay::ConsistencyChecker::OverlayProbe::RunShardsFunc NO_SHARDS;

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

    /* The draw of a structure in a pass: the pass is the one of the draw that the picture is made of unless the test says another. */
    std::string make_candidate(const char*        in_structure_key_ptr,
                               const bool&        in_has_frame,
                               const std::string& in_depth_text = std::string(),
                               const char*        in_pass_key_ptr = MAIN_PASS_TEXT,
                               const uint32_t&    in_frame_index = 7)
    {
        const std::string frame_text = (in_has_frame) ? "\"first_frame_index\":" + std::to_string(in_frame_index) + ","
                                                      : std::string();

        return std::string("{\"signature_key\":\"0x00000000000000aa\",\"structure_key\":\"") + in_structure_key_ptr + "\",\"pass_id\":1,\"pass_key\":\"" + in_pass_key_ptr +
               "\",\"n_frames\":3," + frame_text + in_depth_text +
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

        /* The draws are in the main pass only: a draw of the same structure that is left out in another pass leaves the picture as it is. */
        for (const auto& pass_draw : in_options.recipe.skipped.pass_draw_vec)
        {
            is_hud_skipped   = (is_hud_skipped   || (pass_draw.draw_key == HUD_KEY   && pass_draw.pass_key == MAIN_PASS_KEY) );
            is_scene_skipped = (is_scene_skipped || (pass_draw.draw_key == SCENE_KEY && pass_draw.pass_key == MAIN_PASS_KEY) );
        }

        /* A pass left out whole takes the draws of the main pass with it. */
        for (const auto pass_key : in_options.recipe.skipped.pass_key_vec)
        {
            is_hud_skipped   = (is_hud_skipped   || pass_key == MAIN_PASS_KEY);
            is_scene_skipped = (is_scene_skipped || pass_key == MAIN_PASS_KEY);
        }

        /* Nothing is left out by the key of a draw alone: the probe names the pass. */
        EXPECT_TRUE(in_options.recipe.skipped.draw_key_vec.empty() );

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
                                                                      NO_SHARDS,
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

    /* The animation is a GIF of the frame and of the frame with the draws in magenta, and the page shows it. */
    EXPECT_EQ(read_text_file(options.dump_directory + "\\candidate_000_frame_7.gif").substr(0, 6),
              "GIF89a");
    EXPECT_TRUE(read_text_file(options.dump_directory + "\\candidate_001_frame_7.gif").empty() );
    EXPECT_NE(list.find("candidate_000_frame_7.gif"), std::string::npos) << list;
    EXPECT_NE(page.find("<img src=\"candidate_000_frame_7.gif\""), std::string::npos);
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
                                                                       NO_SHARDS,
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
                                                                      NO_SHARDS,
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

TEST(OverlayProbe, ADrawIsLeftOutInItsPassAndNotInThePassesWhichHaveADrawOfItsStructure)
{
    Tests::TempDirectory                        directory;
    Stereo::Replay::ConsistencyChecker::Options options;
    std::string                                 error;

    /* One structure of draw in two passes: in the main pass it makes a pixel, in the other the picture has nothing of it (a scene may be drawn by a pass that shares it). */
    write_text_file(directory.get_path() + "\\candidates_in.json",
                    "{\"candidates\":[" + make_candidate(HUD_TEXT, true, std::string(), MAIN_PASS_TEXT)  + "," +
                                         make_candidate(HUD_TEXT, true, std::string(), OTHER_PASS_TEXT) + "]}");

    options.dump_directory         = directory.get_path() + "\\out";
    options.overlay_probe_filename = directory.get_path() + "\\candidates_in.json";

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::OverlayProbe::run(options,
                                                                      render,
                                                                      NO_SHARDS,
                                                                     &error) ) << error;

    const std::string list = read_text_file(options.dump_directory + "\\candidates.json");

    EXPECT_FALSE(read_text_file(options.dump_directory + "\\candidate_000_frame_7.png").empty() );
    EXPECT_TRUE (read_text_file(options.dump_directory + "\\candidate_001_frame_7.png").empty() );
    EXPECT_NE(list.find("\"pass_key\": \"0x0000000000000099\""), std::string::npos) << list;
    EXPECT_NE(list.find("\"pass_key\": \"0x0000000000000077\""), std::string::npos) << list;
}

TEST(OverlayProbe, AListWhoseCandidatesDoNotSayWhichPassTheyAreOfFails)
{
    Tests::TempDirectory                        directory;
    Stereo::Replay::ConsistencyChecker::Options options;
    std::string                                 error;

    write_text_file(directory.get_path() + "\\old.json",
                    "{\"candidates\":[{\"signature_key\":\"0x00000000000000aa\",\"structure_key\":\"0x0000000000001111\",\"pass_id\":1,\"n_frames\":3,\"first_frame_index\":7,"
                    "\"agreement\":\"both\",\"replay_class\":\"unaffected\",\"live_class\":\"unaffected\",\"reasons\":[]}]}");

    options.dump_directory         = directory.get_path() + "\\out";
    options.overlay_probe_filename = directory.get_path() + "\\old.json";

    EXPECT_FALSE(Stereo::Replay::ConsistencyChecker::OverlayProbe::run(options,
                                                                       render,
                                                                       NO_SHARDS,
                                                                      &error) );
    EXPECT_NE(error.find("pass_key"), std::string::npos) << error;
}

namespace
{
    /* Which part of the probe replayed which frame: the part that asked, and the frame, in the order they were asked for. */
    struct ReplayRecord
    {
        std::vector<std::pair<uint32_t, uint32_t> > shard_frame_vec;
    };

    /* The render function above, which also says that it was asked. */
    Stereo::Replay::ConsistencyChecker::OverlayProbe::RenderFunc make_recording_render_func(ReplayRecord* out_record_ptr)
    {
        return [out_record_ptr](const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                const uint32_t&                                    in_frame_index,
                                Stereo::Replay::FrameImages::Image*                out_image_ptr,
                                std::string*                                       out_error_ptr)
        {
            out_record_ptr->shard_frame_vec.push_back(std::make_pair(in_options.overlay_probe_shard_index,
                                                                     in_frame_index) );

            return render(in_options,
                          in_frame_index,
                          out_image_ptr,
                          out_error_ptr);
        };
    }
}

TEST(OverlayProbe, TheFramesAreSharedBetweenThePartsAFrameAtATimeAndTheResultsAreOneListInTheOrderOfTheCandidates)
{
    Tests::TempDirectory                        directory;
    Stereo::Replay::ConsistencyChecker::Options options;
    ReplayRecord                                record;
    std::string                                 error;
    uint32_t                                    n_shards_asked = 0;

    /* Three frames of one draw each, and two jobs. */
    write_text_file(directory.get_path() + "\\candidates_in.json",
                    "{\"candidates\":[" + make_candidate(HUD_TEXT, true, std::string(), MAIN_PASS_TEXT, 9) + "," +
                                         make_candidate(HUD_TEXT, true, std::string(), MAIN_PASS_TEXT, 7) + "," +
                                         make_candidate(HUD_TEXT, true, std::string(), MAIN_PASS_TEXT, 8) + "]}");

    options.dump_directory         = directory.get_path() + "\\out";
    options.n_jobs                 = 2;
    options.overlay_probe_filename = directory.get_path() + "\\candidates_in.json";

    /* The parts are run one after the other here, as the processes that run them in the program would, each with options of its own. */
    const auto run_shards_func = [&](const uint32_t&                 in_n_shards,
                                     const std::vector<std::string>& in_result_filename_vec,
                                     std::string*                    out_part_error_ptr)
    {
        bool result = true;

        n_shards_asked = in_n_shards;

        for (uint32_t n_shard = 0;
                      n_shard < in_n_shards;
                    ++n_shard)
        {
            Stereo::Replay::ConsistencyChecker::Options part_options = options;

            part_options.n_jobs                        = 1;
            part_options.overlay_probe_result_filename = in_result_filename_vec.at(n_shard);
            part_options.overlay_probe_shard_count     = in_n_shards;
            part_options.overlay_probe_shard_index     = n_shard;

            result = (Stereo::Replay::ConsistencyChecker::OverlayProbe::run(part_options,
                                                                            make_recording_render_func(&record),
                                                                            NO_SHARDS,
                                                                            out_part_error_ptr) && result);
        }

        return result;
    };

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::OverlayProbe::run(options,
                                                                      make_recording_render_func(&record),
                                                                      run_shards_func,
                                                                     &error) ) << error;

    EXPECT_EQ(n_shards_asked,
              2u);

    /* The latest frame costs most and has a part to itself; the other two share the other part (in the order of the list), and each frame is made twice (as it
     * is, and without the draw).
     */
    std::vector<std::pair<uint32_t, uint32_t> > expected_vec;

    expected_vec.push_back(std::make_pair(0u, 9u) );
    expected_vec.push_back(std::make_pair(0u, 9u) );
    expected_vec.push_back(std::make_pair(1u, 7u) );
    expected_vec.push_back(std::make_pair(1u, 7u) );
    expected_vec.push_back(std::make_pair(1u, 8u) );
    expected_vec.push_back(std::make_pair(1u, 8u) );

    EXPECT_EQ(record.shard_frame_vec,
              expected_vec);

    /* The list is of all of the candidates, in the order of the list that was given, and the files are named by the place of the candidate in it. */
    const std::string list = read_text_file(options.dump_directory + "\\candidates.json");

    EXPECT_NE(list.find("candidate_000_frame_9.gif"), std::string::npos) << list;
    EXPECT_NE(list.find("candidate_001_frame_7.gif"), std::string::npos) << list;
    EXPECT_NE(list.find("candidate_002_frame_8.gif"), std::string::npos) << list;
    EXPECT_LT(list.find("candidate_000_frame_9.gif"),
              list.find("candidate_001_frame_7.gif") );
    EXPECT_LT(list.find("candidate_001_frame_7.gif"),
              list.find("candidate_002_frame_8.gif") );
    EXPECT_EQ(read_text_file(options.dump_directory + "\\candidate_002_frame_8.gif").substr(0, 6),
              "GIF89a");

    /* The results of the parts are not left behind, and the page has the three. */
    EXPECT_TRUE(read_text_file(options.dump_directory + "\\part_0.json").empty() );
    EXPECT_TRUE(read_text_file(options.dump_directory + "\\part_1.json").empty() );
    EXPECT_NE(read_text_file(options.dump_directory + "\\index.html").find("candidate_002_frame_8.gif"),
              std::string::npos);
}

TEST(OverlayProbe, ThereAreNoMorePartsThanFramesAndOneFrameIsDoneWhereItIs)
{
    Tests::TempDirectory                        directory;
    Stereo::Replay::ConsistencyChecker::Options options;
    ReplayRecord                                record;
    std::string                                 error;
    bool                                        was_asked = false;

    write_text_file(directory.get_path() + "\\candidates_in.json",
                    "{\"candidates\":[" + make_candidate(HUD_TEXT,    true) + "," +
                                         make_candidate(HIDDEN_TEXT, true) + "]}");

    options.dump_directory         = directory.get_path() + "\\out";
    options.n_jobs                 = 4;
    options.overlay_probe_filename = directory.get_path() + "\\candidates_in.json";

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::OverlayProbe::run(options,
                                                                      make_recording_render_func(&record),
                                                                      [&](const uint32_t&,
                                                                          const std::vector<std::string>&,
                                                                          std::string*)
                                                                      {
                                                                          was_asked = true;

                                                                          return false;
                                                                      },
                                                                     &error) ) << error;

    EXPECT_FALSE(was_asked);
    EXPECT_FALSE(record.shard_frame_vec.empty() );
}

TEST(OverlayProbe, APassWhichMakesNoDifferenceLeftOutWholeIsLeftOutOnceForAllItsDraws)
{
    Tests::TempDirectory                        directory;
    Stereo::Replay::ConsistencyChecker::Options options;
    ReplayRecord                                record;
    ReplayRecord                                visible_record;
    std::string                                 error;

    /* Two draws in a pass that makes no pixels: the picture is made of the draws of the main pass. */
    write_text_file(directory.get_path() + "\\candidates_in.json",
                    "{\"candidates\":[" + make_candidate(HIDDEN_TEXT, true, std::string(), OTHER_PASS_TEXT) + "," +
                                         make_candidate(SCENE_TEXT,  true, std::string(), OTHER_PASS_TEXT) + "]}");

    options.dump_directory         = directory.get_path() + "\\out";
    options.n_jobs                 = 1;
    options.overlay_probe_filename = directory.get_path() + "\\candidates_in.json";

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::OverlayProbe::run(options,
                                                                      make_recording_render_func(&record),
                                                                      NO_SHARDS,
                                                                     &error) ) << error;

    /* The frame as it is, and the pass left out: no more, however many draws the pass has. */
    EXPECT_EQ(record.shard_frame_vec.size(),
              2u);

    const std::string list = read_text_file(options.dump_directory + "\\candidates.json");

    EXPECT_EQ(list.find("candidate_000"), std::string::npos) << list;
    EXPECT_EQ(list.find("candidate_001"), std::string::npos) << list;
    EXPECT_NE(list.find("\"n_picture_pixels\": 8"), std::string::npos) << list;

    /* A pass that does make a difference has its draws probed one by one, after the pass. */
    write_text_file(directory.get_path() + "\\candidates_in.json",
                    "{\"candidates\":[" + make_candidate(HUD_TEXT,    true) + "," +
                                         make_candidate(HIDDEN_TEXT, true) + "]}");

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::OverlayProbe::run(options,
                                                                      make_recording_render_func(&visible_record),
                                                                      NO_SHARDS,
                                                                     &error) ) << error;

    /* The frame as it is, the pass, and each of the two draws. */
    EXPECT_EQ(visible_record.shard_frame_vec.size(),
              4u);
    EXPECT_FALSE(read_text_file(options.dump_directory + "\\candidate_000_frame_7.png").empty() );
    EXPECT_TRUE (read_text_file(options.dump_directory + "\\candidate_001_frame_7.png").empty() );
}

TEST(OverlayProbe, ThePassesOfOneFrameAreSharedBetweenThePartsTooAndTheDrawsOfOnePassAreNot)
{
    Tests::TempDirectory                        directory;
    Stereo::Replay::ConsistencyChecker::Options options;
    ReplayRecord                                record;
    std::string                                 error;

    /* One frame, with a draw in each of four passes, and two draws in a fifth: the frame is the one that costs most, and a part which waited for all of them would
     * be the last to be done.
     */
    write_text_file(directory.get_path() + "\\candidates_in.json",
                    "{\"candidates\":[" + make_candidate(HUD_TEXT,    true, std::string(), "0x0000000000000011") + "," +
                                         make_candidate(HUD_TEXT,    true, std::string(), "0x0000000000000012") + "," +
                                         make_candidate(HUD_TEXT,    true, std::string(), "0x0000000000000013") + "," +
                                         make_candidate(HUD_TEXT,    true, std::string(), "0x0000000000000014") + "," +
                                         make_candidate(HIDDEN_TEXT, true, std::string(), OTHER_PASS_TEXT)      + "," +
                                         make_candidate(SCENE_TEXT,  true, std::string(), OTHER_PASS_TEXT)      + "]}");

    options.dump_directory         = directory.get_path() + "\\out";
    options.n_jobs                 = 2;
    options.overlay_probe_filename = directory.get_path() + "\\candidates_in.json";

    const auto run_shards_func = [&](const uint32_t&                 in_n_shards,
                                     const std::vector<std::string>& in_result_filename_vec,
                                     std::string*                    out_part_error_ptr)
    {
        bool result = true;

        for (uint32_t n_shard = 0;
                      n_shard < in_n_shards;
                    ++n_shard)
        {
            Stereo::Replay::ConsistencyChecker::Options part_options = options;

            part_options.n_jobs                        = 1;
            part_options.overlay_probe_result_filename = in_result_filename_vec.at(n_shard);
            part_options.overlay_probe_shard_count     = in_n_shards;
            part_options.overlay_probe_shard_index     = n_shard;

            result = (Stereo::Replay::ConsistencyChecker::OverlayProbe::run(part_options,
                                                                            make_recording_render_func(&record),
                                                                            NO_SHARDS,
                                                                            out_part_error_ptr) && result);
        }

        return result;
    };

    ASSERT_TRUE(Stereo::Replay::ConsistencyChecker::OverlayProbe::run(options,
                                                                      make_recording_render_func(&record),
                                                                      run_shards_func,
                                                                     &error) ) << error;

    /* Both parts replayed the frame (each made it as it is for its own candidates). */
    size_t n_replays_of_part_0 = 0;
    size_t n_replays_of_part_1 = 0;

    for (const auto& shard_frame : record.shard_frame_vec)
    {
        n_replays_of_part_0 += (shard_frame.first == 0) ? 1 : 0;
        n_replays_of_part_1 += (shard_frame.first == 1) ? 1 : 0;
    }

    EXPECT_GT(n_replays_of_part_0,
              0u);
    EXPECT_GT(n_replays_of_part_1,
              0u);

    /* The two draws of the fifth pass are in one part: the pass is left out once for them (the frame as it is, the pass; no more for the two). */
    const std::string list = read_text_file(options.dump_directory + "\\candidates.json");

    EXPECT_EQ(list.find("candidate_004"), std::string::npos) << list;
    EXPECT_EQ(list.find("candidate_005"), std::string::npos) << list;

    /* The replays add up: a draw each of the four passes (four), the pass of two draws (one), and the frame as it is, once in each of the two parts. */
    EXPECT_EQ(record.shard_frame_vec.size(),
              7u);
}

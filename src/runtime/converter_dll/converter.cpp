/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/recipe/recipe_loader.h"
#include "capture/api_dump_loader/api_info.h"
#include "common/frame_boundary.h"
#include "common/vtable.h"
#include "interception/hooks/D3D8/types.h"
#include "stereo/session/session_environment.h"
#include "converter.h"
#include "environment.h"
#include "own_call_scope.h"
#include "scoped_tick_adder.h"
#include <array>
#include <set>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>

Stereo::ConverterDLL::Converter::Converter(const Stereo::Session::SessionInfo&              in_session_info,
                                           const Analysis::Recipe::Recipe&                  in_recipe,
                                           std::unique_ptr<Stereo::Transport::FrameChannel> in_channel_ptr)
    :m_channel_ptr(std::move(in_channel_ptr) ),
     m_log        (in_session_info)
{
    const auto write_failure_func = [this](const std::string& in_message)
    {
        m_log.write_failure("The application cannot be converted: " + in_message);

        ::TerminateProcess(::GetCurrentProcess(),
                           Stereo::Session::SessionEnvironment::get_converter_failed_exit_code() );
    };

    /* A converter that finds that it cannot convert the application right ends it, and says why in the log. */
    m_live_state.core_ptr.reset(new Stereo::Core::StereoCore(in_recipe,
                                                             write_failure_func) );

    /* The calls that the feeder follows (Direct3D 7, Direct3D 8 and OpenGL), before and after. A Direct3D 8 frame is passed on before
     * the present, since what the back buffer holds after a present is not defined.
     */
    for (uint32_t n_api_func = 0;
                  n_api_func < APIInterceptor::APIFUNCTION_COUNT;
                ++n_api_func)
    {
        const auto function = static_cast<APIInterceptor::APIFunction>(n_api_func);

        if (CallFeeder::is_followed(function) )
        {
            APIInterceptor::register_for_post_callback(function,
                                                      &Converter::on_post_callback,
                                                       this);
            APIInterceptor::register_for_pre_callback (function,
                                                      &Converter::on_pre_callback,
                                                       this);
        }
    }

    /* Ways to set what the program which shows the images says otherwise, for a run without it, and to look at a frame. */
    {
        float value = 0.0f;

        if (Environment::get_number("APISTEREO_EYE_SEPARATION",
                                    &value) )
        {
            m_live_state.core_ptr->set_eye_separation(value);

            m_log.write("The eye separation is "           +
                        std::to_string(value)              +
                        ", from APISTEREO_EYE_SEPARATION");
        }

        if (Environment::get_number("APISTEREO_DEPTH_STRENGTH",
                                    &value) )
        {
            m_live_state.core_ptr->set_depth_strength(value);

            m_log.write("The strength of the 3D is "       +
                        std::to_string(value)              +
                        ", from APISTEREO_DEPTH_STRENGTH");
        }

        if (Environment::get_number("APISTEREO_PUSH_OVERLAY",
                                    &value) )
        {
            m_live_state.core_ptr->set_is_overlay_pushed(value != 0.0f);

            m_live_state.is_overlay_push_forced = true;

            m_log.write("What the recipe leaves alone is pushed toward the viewer: " +
                        std::to_string(value != 0.0f)                                +
                        ", from APISTEREO_PUSH_OVERLAY");
        }

        if (Environment::get_number("APISTEREO_DUMP_EVERY",
                                    &value) )
        {
            m_live_state.dump_every = static_cast<uint64_t>(value);
        }

        if (Environment::get_number("APISTEREO_DUMP_FIRST_FRAME",
                                    &value) )
        {
            m_live_state.dump_first_frame = static_cast<uint64_t>(value);
        }

        if (Environment::get_number("APISTEREO_DUMP_FRAME",
                                    &value) )
        {
            m_live_state.dump_frame = static_cast<uint64_t>(value);
        }

        m_live_state.is_scene_dump_enabled = Environment::is_set("APISTEREO_DUMP_SCENES");
    }

    m_live_state.d3d7_renderer_ptr.reset(new D3D7EyeRenderer(m_live_state.core_ptr.get(),
                                                            &m_live_state.d3d7_windowed_mode) );
    m_live_state.d3d8_renderer_ptr.reset(new D3D8EyeRenderer(m_live_state.core_ptr.get() ));

    {
        std::unique_ptr<Stereo::Transport::IFrameExporter> d3d7_exporter_ptr(new D3D7HostMemoryExporter(m_channel_ptr.get() ));
        std::unique_ptr<Stereo::Transport::IFrameExporter> d3d8_exporter_ptr(new D3D8HostMemoryExporter(m_channel_ptr.get() ));

        m_frame_passer_ptr.reset     (new FramePasser(std::move(d3d8_exporter_ptr),
                                                      m_log) );
        m_d3d7_frame_passer_ptr.reset(new FramePasser(std::move(d3d7_exporter_ptr),
                                                      m_log) );
    }

    {
        /* Said by the program which shows the images, before it starts the application: the device is made with it. */
        uint32_t target_height = 0;
        uint32_t target_width  = 0;

        bool is_shown_elsewhere = m_channel_ptr->get_target_eye_size(&target_width,
                                                                     &target_height);

        /* A way to say it for a run without VRPlayer, which says it otherwise. */
        {
            float height = 0.0f;
            float width  = 0.0f;

            if (is_shown_elsewhere == false)
            {
                const bool has_width = Environment::get_number("APISTEREO_TARGET_EYE_WIDTH",
                                                               &width);

                if (has_width)
                {
                    const bool has_height = Environment::get_number("APISTEREO_TARGET_EYE_HEIGHT",
                                                                    &height);

                    if (has_height)
                    {
                        target_height      = static_cast<uint32_t>(height);
                        target_width       = static_cast<uint32_t>(width);
                        is_shown_elsewhere = true;

                        m_log.write("The images of the eyes are " + std::to_string(target_width) + " x " + std::to_string(target_height) + ", from APISTEREO_TARGET_EYE_WIDTH and APISTEREO_TARGET_EYE_HEIGHT");
                    }
                }
            }
        }

        /* A program that shows the images says how large they are before the application starts: without one the application is on the screen, as
         * it is, and a window of it is not made invisible.
         */
        m_live_state.d3d7_windowed_mode.set_target_eye_size(target_width,
                                                            target_height);
        m_live_state.d3d7_windowed_mode.set_is_enabled     (is_shown_elsewhere);

        m_live_state.gl_renderer_ptr.reset(new GLEyeRenderer(m_live_state.core_ptr.get(),
                                                             is_shown_elsewhere,
                                                             target_width,
                                                             target_height) );

        const auto get_gl_chunk_func = [this](const uint32_t& in_chunk_id,
                                              const void**    out_data_ptr_ptr,
                                              uint32_t*       out_n_bytes_ptr)
        {
            return m_live_state.feeder.get_last_call_chunk(in_chunk_id,
                                                           out_data_ptr_ptr,
                                                           out_n_bytes_ptr);
        };

        m_live_state.gl_renderer_ptr->set_get_chunk_func(get_gl_chunk_func);

        {
            std::unique_ptr<Stereo::Transport::IFrameExporter> gl_exporter_ptr(new GLHostMemoryExporter(m_channel_ptr.get() ));

            m_gl_frame_passer_ptr.reset(new FramePasser(std::move(gl_exporter_ptr),
                                                        m_log) );
        }

        const auto get_render_target_func = [this]()
        {
            return m_live_state.d3d8_renderer_ptr->get_render_target_ptr();
        };

        const auto write_log_func = [this](const std::string& in_message)
        {
            m_log.write(in_message);
        };

        m_live_state.scaler_ptr.reset(new D3D8ResolutionScaler(target_width,
                                                               target_height,
                                                               m_live_state.core_ptr.get(),
                                                               get_render_target_func,
                                                               write_log_func) );

        const auto restore_chunk_func = [this](const APIInterceptor::APIFunction& in_function,
                                               std::vector<uint8_t>*              inout_chunk_ptr)
        {
            m_live_state.scaler_ptr->restore_chunk(in_function,
                                                   inout_chunk_ptr);
        };

        /* The analysis sees the application as it asked to be, or the signatures of its draws would depend on the scale. */
        m_live_state.feeder.set_chunk_func(restore_chunk_func);
    }

    for (const auto& note : Analysis::Recipe::RecipeLoader::describe_pass_selectors_live_ignores(in_recipe) )
    {
        m_log.write("Warning: " +
                    note);
    }

    m_log.write("The converter is loaded");
}

Stereo::ConverterDLL::Converter::~Converter()
{
    m_log.write("The converter is removed");
}

void Stereo::ConverterDLL::Converter::count_draw(const Analysis::Recipe::DrawClass& in_draw_class)
{
    switch (in_draw_class)
    {
        case Analysis::Recipe::DrawClass::SKIPPED:
        {
            m_live_state.n_skipped_draws++;

            break;
        }

        case Analysis::Recipe::DrawClass::UNAFFECTED:
        {
            m_live_state.n_unaffected_draws++;

            break;
        }

        case Analysis::Recipe::DrawClass::AFFECTED:
        default:
        {
            m_live_state.n_affected_draws++;

            break;
        }
    }
}

void Stereo::ConverterDLL::Converter::count_frame(std::vector<std::string>* out_log_line_vec_ptr)
{
    constexpr uint64_t N_FRAMES_BETWEEN_STATISTICS = 600;

    m_live_state.n_frames++;


    /* The first frame says that the calls of the application reach the converter, and how far the application has come. */
    if (m_live_state.n_frames == 1)
    {
        out_log_line_vec_ptr->push_back("The first frame ends, after " + std::to_string(m_live_state.n_affected_draws + m_live_state.n_unaffected_draws + m_live_state.n_skipped_draws) + " draws");
    }

    if (m_live_state.n_frames % N_FRAMES_BETWEEN_STATISTICS == 0)
    {
        out_log_line_vec_ptr->push_back(make_statistics_line() );
    }

    read_channel_settings(out_log_line_vec_ptr);
}

std::unique_ptr<Stereo::ConverterDLL::Converter> Stereo::ConverterDLL::Converter::create_ptr()
{
    std::string                  error;
    std::unique_ptr<Converter>   result_ptr;
    Stereo::Session::SessionInfo session_info;

    if (Stereo::Session::SessionEnvironment::read(&session_info) )
    {
        Analysis::Recipe::Recipe recipe;
        auto                     channel_ptr = Stereo::Transport::FrameChannel::open_ptr(session_info.session_id,
                                                                                        &error);

        if (channel_ptr != nullptr)
        {
            const bool is_recipe_loaded = Analysis::Recipe::RecipeLoader::load(session_info.recipe_path,
                                                                               &recipe,
                                                                               &error);

            if (is_recipe_loaded)
            {
                result_ptr.reset(new Converter(session_info,
                                               recipe,
                                               std::move(channel_ptr) ));
            }
        }
    }

    return result_ptr;
}

std::string Stereo::ConverterDLL::Converter::make_statistics_line() const
{
    /* The application is of one API, so one of the two renderers has the twins. */
    const uint64_t n_failed_twins = m_live_state.d3d7_renderer_ptr->get_n_failed_twins() + m_live_state.d3d8_renderer_ptr->get_n_failed_twins();
    const uint64_t n_twins        = m_live_state.d3d7_renderer_ptr->get_n_twins       () + m_live_state.d3d8_renderer_ptr->get_n_twins       ();
    LARGE_INTEGER  frequency;

    ::QueryPerformanceFrequency(&frequency);

    const double      n_frames_d = static_cast<double>((m_live_state.n_frames > 0) ? m_live_state.n_frames : 1);
    const std::string cost_text  = ", the callbacks took "                                                                                                                    +
                                   std::to_string(1000.0 * static_cast<double>(m_live_state.n_callback_ticks.load() ) / static_cast<double>(frequency.QuadPart) / n_frames_d) +
                                   " ms a frame, "                                                                                                                            +
                                   std::to_string(1000.0 * static_cast<double>(m_live_state.n_draw_ticks.load    () ) / static_cast<double>(frequency.QuadPart) / n_frames_d) +
                                   " of them in the draws";
    const std::string twin_text  = (n_twins + n_failed_twins > 0) ? ", "                                                                      +
                                                                    std::to_string(n_twins)                                                   +
                                                                    " surfaces with a twin, "                                                 +
                                                                    std::to_string(n_failed_twins)                                            +
                                                                    " twins that could not be made, "                                         +
                                                                    std::to_string(m_live_state.d3d7_renderer_ptr->get_n_unmirrored_calls() ) +
                                                                    " calls not made for the right eye"                                       +
                                                                    ", "                                                                      +
                                                                    std::to_string(m_live_state.d3d7_renderer_ptr->get_n_pixel_calls() )      +
                                                                    " calls on the back buffer in pixels it no longer has"
                                                                  : std::string();

    const auto&       gl_stats  = m_live_state.gl_renderer_ptr->get_statistics();
    const std::string gl_text   = (m_live_state.gl_renderer_ptr->is_ready() ) ? ", OpenGL: "                                                  +
                                                                                    std::to_string(gl_stats.n_block_draws)             +
                                                                                    " blocks, "                                        +
                                                                                    std::to_string(gl_stats.n_array_draws)             +
                                                                                    " array draws, "                                    +
                                                                                    std::to_string(gl_stats.n_list_draws)              +
                                                                                    " list calls, "                                     +
                                                                                    std::to_string(gl_stats.n_skipped_draws)           +
                                                                                    " skipped, "                                        +
                                                                                    std::to_string(gl_stats.n_camera_attached_draws)   +
                                                                                    " attached to the camera, "                         +
                                                                                    std::to_string(gl_stats.n_clears)                  +
                                                                                    " clears, "                                         +
                                                                                    std::to_string(gl_stats.n_texture_copies)          +
                                                                                    " texture copies, "                                 +
                                                                                    std::to_string(gl_stats.n_twin_textures)           +
                                                                                    " twin textures, "                                  +
                                                                                    std::to_string(gl_stats.n_unmirrored_calls)        +
                                                                                    " calls made for the left eye only"
                                                                                  : std::string();

    return "After "                                                       +
           std::to_string(m_live_state.n_frames)                          +
           " frames: "                                                    +
           std::to_string(m_live_state.n_affected_draws)                  +
           " affected draws, "                                            +
           std::to_string(m_live_state.n_unaffected_draws)                +
           " unaffected, "                                                +
           std::to_string(m_live_state.n_skipped_draws)                   +
            " skipped, "                                                  +
           std::to_string(m_live_state.core_ptr->get_n_unknown_draws() )  +
           " of a signature the recipe does not know, "                   +
           std::to_string(m_live_state.core_ptr->get_n_unknown_frames() ) +
           " frames of a scene it has no distance for, "                  +
           std::to_string(m_live_state.n_kept_matrices)                   +
           " matrices kept"                                               +
           twin_text                                                      +
           gl_text                                                        +
           cost_text;
}

void Stereo::ConverterDLL::Converter::on_d3d7_frame_ending(const APIInterceptor::APIFunction&         in_api_func,
                                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                           std::vector<std::string>*                  out_log_line_vec_ptr)
{
    /* The frame which a flip is about to show is in the back buffer of the surface it is called for. */
    if (in_api_func == APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP)
    {
        pass_d3d7_frame(const_cast<void*>(in_arguments_ptr[0].get_ptr() ),
                        nullptr); /* in_back_buffer_ptr: the one the primary surface has */

        m_live_state.d3d7_windowed_mode.keep_windows_invisible();
    }
    else
    /* A program of a window shows its frame by a blit of the surface it rendered to onto the primary surface: that surface is the picture. */
    if (in_api_func == APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT     ||
        in_api_func == APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST)
    {
        constexpr uint32_t SOURCE_ARGUMENT = 2;

        pass_d3d7_frame(const_cast<void*>(in_arguments_ptr[0].get_ptr() ),
                        const_cast<void*>(in_arguments_ptr[SOURCE_ARGUMENT].get_ptr() ) );

        m_live_state.d3d7_windowed_mode.keep_windows_invisible();
    }

    /* What was not made for the right eye is said, the first calls of each. */
    for (const auto& note : m_live_state.d3d7_renderer_ptr->take_unmirrored_notes() )
    {
        out_log_line_vec_ptr->push_back(note);
    }

    count_frame(out_log_line_vec_ptr);
}

bool Stereo::ConverterDLL::Converter::on_gl_draws(Analysis::Recipe::DrawClass* out_draw_class_ptr,
                                                  bool*                        out_is_pushed_ptr,
                                                  std::vector<std::string>*    out_log_line_vec_ptr)
{
    std::set<Analysis::Recipe::DrawClass> class_set;
    bool                                  is_every_draw_pushed = true;

    for (const auto& draw_keys : m_live_state.feeder.get_last_call_draw_key_vec() )
    {
        const auto draw_class = m_live_state.core_ptr->classify_draw(draw_keys.signature_key,
                                                                     draw_keys.cover_key);

        class_set.insert(draw_class);

        count_draw(draw_class);

        is_every_draw_pushed = is_every_draw_pushed &&
                               m_live_state.core_ptr->get_is_last_draw_pushed();
    }

    *out_is_pushed_ptr = (class_set.empty() == false &&
                          is_every_draw_pushed);

    /* The draws of a display list are made by the driver, in one call: they cannot be made for an eye one at a time, so a list whose draws are not all of one class is
     * told (the first one), and counted.
     */
    if (class_set.size() > 1)
    {
        if (m_live_state.n_mixed_class_calls == 0)
        {
            out_log_line_vec_ptr->push_back("A call of a display list made draws that the recipe makes different things of: it cannot be made for an eye one draw at a time.");
        }

        m_live_state.n_mixed_class_calls++;
    }

    /* The call is made as affected if any of its draws is: a list that has a draw of the scene is the scene's. */
    if (class_set.find(Analysis::Recipe::DrawClass::AFFECTED) != class_set.end() )
    {
        *out_draw_class_ptr = Analysis::Recipe::DrawClass::AFFECTED;
    }
    else
    if (class_set.find(Analysis::Recipe::DrawClass::UNAFFECTED) != class_set.end() )
    {
        *out_draw_class_ptr = Analysis::Recipe::DrawClass::UNAFFECTED;
    }
    else
    {
        *out_draw_class_ptr = Analysis::Recipe::DrawClass::SKIPPED;
    }

    return (class_set.empty() == false);
}

void Stereo::ConverterDLL::Converter::on_post_callback(APIInterceptor::APIFunction                in_api_func,
                                                       void*                                      in_user_arg_ptr,
                                                       uint32_t                                   in_n_arguments_out,
                                                       const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr,
                                                       const APIInterceptor::APIFunctionArgument* in_returned_value_ptr)
{
    auto* this_ptr = static_cast<Converter*>(in_user_arg_ptr);

    if (OwnCallScope::is_active() == false)
    {
        std::lock_guard<Utils::Parallel::SrwMutex> lock(this_ptr->m_live_state.mutex);

        uint64_t shape_key = 0;

        this_ptr->m_live_state.feeder.on_post(in_api_func,
                                              in_n_arguments_out,
                                              in_arguments_out_ptr,
                                              in_returned_value_ptr);

        /* What the frame that ended was made of tells which scene it is; the draws that follow are converged for it. */
        if (this_ptr->m_live_state.feeder.take_completed_frame_shape_key(&shape_key) )
        {
            /* The eyes follow a scene by the time that goes, not by the number of frames: how long this frame took after the one before says how far they go. */
            LARGE_INTEGER frame_qpc;
            LARGE_INTEGER frequency;

            ::QueryPerformanceCounter  (&frame_qpc);
            ::QueryPerformanceFrequency(&frequency);

            if (this_ptr->m_live_state.last_frame_qpc != 0)
            {
                const double frame_seconds = static_cast<double>(frame_qpc.QuadPart - this_ptr->m_live_state.last_frame_qpc) / static_cast<double>(frequency.QuadPart);

                this_ptr->m_live_state.core_ptr->set_frame_interval_seconds(static_cast<float>(frame_seconds) );
            }

            this_ptr->m_live_state.last_frame_qpc = frame_qpc.QuadPart;

            /* How deep the frame was, as the application drew it, if the recipe has it measured (see Comfort::measures_depth); else what was baked for the kind of frame. */
            Stereo::Core::DepthRange depth_range;
            float                    projection_scale = 0.0f;

            /* The depth that is given is of the frame before when the depth buffer is read to a buffer of the graphics card, which is not waited for (see
             * GLEyeRenderer::get_depth_lag()): it goes with the scene that frame was of.
             */
            const uint32_t depth_lag       = this_ptr->m_live_state.gl_renderer_ptr->get_depth_lag();
            const uint64_t depth_shape_key = (depth_lag == 0) ? shape_key
                                                              : this_ptr->m_live_state.previous_shape_key;

            const bool is_measured = (this_ptr->m_live_state.gl_renderer_ptr->take_frame_depth(&depth_range,
                                                                                                &projection_scale) &&
                                      this_ptr->m_live_state.core_ptr->on_frame_depth      (depth_shape_key,
                                                                                            depth_range,
                                                                                            projection_scale) );

            this_ptr->m_live_state.previous_shape_key = shape_key;


            if (is_measured == false)
            {
                this_ptr->m_live_state.core_ptr->on_frame_completed(shape_key);
            }
            else
            {
                /* The scene is told too: the clamp of a part (see StereoCore::plan_part_shift) is the limit of the scene that the draws are in. */
                this_ptr->m_live_state.core_ptr->set_scene_shape_key(shape_key);

                this_ptr->m_live_state.last_depth_range       = depth_range;
                this_ptr->m_live_state.last_projection_scale  = projection_scale;
            }

            /* A debugging aid for tuning a recipe: a frame of each scene, to look at, with the key that the recipe knows it by. It is taken once the run of frames of the scene has lasted
             * long enough for the eyes to have come to what the scene says (they follow it over a fraction of a second, by the time that goes, whatever the frames per second are), so that
             * the cuts of a frame or two are left out.
             */
            constexpr double N_SECONDS_IN_A_SCENE_TO_SETTLE = 1.5;

            if (shape_key != this_ptr->m_live_state.scene_run_key)
            {
                LARGE_INTEGER run_begin_qpc = {};

                ::QueryPerformanceCounter(&run_begin_qpc);

                this_ptr->m_live_state.scene_run_begin_qpc = run_begin_qpc.QuadPart;
                this_ptr->m_live_state.scene_run_key       = shape_key;
                this_ptr->m_live_state.scene_run_n_samples = 0;
            }

            /* A scene that goes on is looked at again, since a scene is not one picture: what the camera shows moves with the time of the demo (half a second apart, whatever the frames per
             * second are). Only the run of the scene in which the first was written goes on.
             */
            constexpr uint32_t N_SAMPLES_OF_A_SCENE      = 60;
            constexpr double   N_SECONDS_BETWEEN_SAMPLES = 0.5;

            bool     is_sampled_now = false;
            uint32_t n_sample       = this_ptr->m_live_state.scene_run_n_samples;

            if (this_ptr->m_live_state.is_scene_dump_enabled &&
                this_ptr->m_live_state.has_last_gl_images)
            {
                LARGE_INTEGER frequency = {};
                LARGE_INTEGER now_qpc   = {};

                ::QueryPerformanceCounter  (&now_qpc);
                ::QueryPerformanceFrequency(&frequency);

                const double n_seconds_in_run       = static_cast<double>(now_qpc.QuadPart - this_ptr->m_live_state.scene_run_begin_qpc)       / static_cast<double>(frequency.QuadPart);
                const double n_seconds_since_sample = static_cast<double>(now_qpc.QuadPart - this_ptr->m_live_state.scene_run_last_sample_qpc) / static_cast<double>(frequency.QuadPart);

                if (n_sample == 0)
                {
                    if (n_seconds_in_run >= N_SECONDS_IN_A_SCENE_TO_SETTLE)
                    {
                        is_sampled_now = this_ptr->m_live_state.scene_seen_set.insert(shape_key).second;
                    }
                }
                else
                if (n_sample               <  N_SAMPLES_OF_A_SCENE      &&
                    n_seconds_since_sample >= N_SECONDS_BETWEEN_SAMPLES)
                {
                    is_sampled_now = true;
                }
            }

            if (is_sampled_now)
            {
                this_ptr->m_live_state.scene_run_n_samples++;

                LARGE_INTEGER sample_qpc = {};

                ::QueryPerformanceCounter(&sample_qpc);

                this_ptr->m_live_state.scene_run_last_sample_qpc = sample_qpc.QuadPart;

                char temp_i8vec260[MAX_PATH] = {};

                if (::GetTempPathA(MAX_PATH,
                                   temp_i8vec260) != 0)
                {
                    char key_i8vec19[19] = {};

                    ::snprintf(key_i8vec19,
                               sizeof(key_i8vec19),
                               "0x%016llx",
                               static_cast<unsigned long long>(shape_key) );

                    const std::string filename = std::string(temp_i8vec260) + "apistereo_scene_" + key_i8vec19 + ( (n_sample == 0) ? std::string() : "_" + std::to_string(n_sample) ) + ".bmp";

                    GLHostMemoryExporter::write_debug_image(this_ptr->m_live_state.last_gl_images,
                                                            filename);

                    this_ptr->m_log.write(std::string("A scene has settled: ") + key_i8vec19 + ", sample " + std::to_string(n_sample) + ", frame " + std::to_string(this_ptr->m_live_state.n_frames) + ", the eyes converge at " + std::to_string(this_ptr->m_live_state.core_ptr->get_effective_convergence_distance() ) + " and are " + std::to_string(this_ptr->m_live_state.core_ptr->get_effective_eye_separation() ) + " apart; measured near " + std::to_string(this_ptr->m_live_state.last_depth_range.near_depth) + ", median " + std::to_string(this_ptr->m_live_state.last_depth_range.median_depth) + ", far " + std::to_string(this_ptr->m_live_state.last_depth_range.far_depth) + ", scale " + std::to_string(this_ptr->m_live_state.last_projection_scale) );
                }
            }
        }

        /* Each API has its own eye renderer, which follows the calls after they are made. */
        const auto api_group = Capture::APIDumpLoader::get_api_group(in_api_func);

        if (api_group == Capture::APIDumpLoader::APIGroup::D3D8)
        {
            this_ptr->m_live_state.d3d8_renderer_ptr->on_post(in_api_func,
                                                              in_n_arguments_out,
                                                              in_arguments_out_ptr);
        }
        else
        if (api_group == Capture::APIDumpLoader::APIGroup::D3D7)
        {
            this_ptr->m_live_state.d3d7_renderer_ptr->on_post(in_api_func,
                                                              in_n_arguments_out,
                                                              in_arguments_out_ptr,
                                                              in_returned_value_ptr);
        }
        else
        {
            /* What is followed and is neither of Direct3D is OpenGL. */
            this_ptr->m_live_state.gl_renderer_ptr->on_post(in_api_func,
                                                            in_n_arguments_out,
                                                            in_arguments_out_ptr);
        }
    }

    /* The frame is presented: the application makes the next one when the screen wants it, which is what the program that shows the
     * frames says (if it does). Outside the lock of the converter, since it is as long as a frame.
     */
    const bool is_own_call = OwnCallScope::is_active();

    if (is_own_call == false                                                     &&
        in_api_func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT)
    {
        this_ptr->m_frame_passer_ptr->wait_for_pace();
    }
    else
    if (is_own_call == false                                                     &&
        in_api_func == APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP)
    {
        this_ptr->m_d3d7_frame_passer_ptr->wait_for_pace();
    }
    else
    if (is_own_call == false                                                    &&
        in_api_func == APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS)
    {
        this_ptr->m_gl_frame_passer_ptr->wait_for_pace();
    }
    else
    if (is_own_call == false)
    {
        const bool is_frame_boundary = APIInterceptor::is_frame_boundary(in_api_func,
                                                                         in_n_arguments_out,
                                                                         in_arguments_out_ptr);

        if (is_frame_boundary                                                                  &&
            (in_api_func      == APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT      ||
             in_api_func      == APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST) )
        {
            this_ptr->m_d3d7_frame_passer_ptr->wait_for_pace();
        }
    }
}

void Stereo::ConverterDLL::Converter::on_pre(const APIInterceptor::APIFunction&         in_api_func,
                                             const uint32_t&                            in_n_arguments,
                                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                             bool*                                      out_should_pass_through_ptr)
{
    const bool               is_present        = (in_api_func    == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT &&
                                                  in_n_arguments >  0);
    GLFrameImages            gl_images;
    bool                     has_gl_images     = false;
    std::vector<std::string> log_line_vec;
    void*                    right_surface_ptr = nullptr;

    {
        std::lock_guard<Utils::Parallel::SrwMutex> lock(m_live_state.mutex);

        const auto api_group = Capture::APIDumpLoader::get_api_group(in_api_func);
        const bool is_d3d8   = (api_group == Capture::APIDumpLoader::APIGroup::D3D8);

        /* What is followed and is neither of Direct3D is OpenGL (and the SwapBuffers of GDI32 that ends its frames). */
        const bool is_gl     = (is_d3d8 == false                                      &&
                                api_group != Capture::APIDumpLoader::APIGroup::D3D7);
        const bool is_draw   = m_live_state.feeder.on_pre(in_api_func,
                                                          in_n_arguments,
                                                          in_arguments_ptr);

        if (in_api_func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM)
        {
            /* The matrix is kept; it is put in the device before the draws that need it, for the eye they are made for. */
            *out_should_pass_through_ptr = (on_set_transform(in_n_arguments,
                                                             in_arguments_ptr) == false);
        }

        if (is_d3d8)
        {
            m_live_state.scaler_ptr->on_pre(in_api_func,
                                            in_n_arguments,
                                            in_arguments_ptr);
        }

        if (is_gl)
        {
            Analysis::Recipe::DrawClass draw_class = Analysis::Recipe::DrawClass::UNAFFECTED;
            bool                        is_pushed  = false;

            /* What is done for a call that draws, down to the end of this block, is the time of the draws. */
            ScopedTickAdder draw_timing(&m_live_state.n_draw_ticks,
                                        is_draw);

            /* The draws of a call of OpenGL are those that the front end made of it (a display list makes several, or none). */
            const bool has_draws = (is_draw) ? on_gl_draws(&draw_class,
                                                           &is_pushed,
                                                           &log_line_vec)
                                             : false;

            if (in_api_func == APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS)
            {
                count_frame(&log_line_vec);

                has_gl_images = m_live_state.gl_renderer_ptr->on_frame_ending(&gl_images);

                m_live_state.has_last_gl_images = has_gl_images;
                m_live_state.last_gl_images     = gl_images;

                /* The copy is for a look at the frame before the next is read, not a hold on its memory: the reader would wait for it. */
                m_live_state.last_gl_images.consumer_token.reset();
            }

            /* What the application renders is made for both eyes: a draw or a clear is made here, and the call of the application is not. */
            if (m_live_state.gl_renderer_ptr->on_pre(in_api_func,
                                                     in_n_arguments,
                                                     in_arguments_ptr,
                                                     has_draws,
                                                     draw_class,
                                                     is_pushed) == false)
            {
                *out_should_pass_through_ptr = false;
            }

            for (const auto& note : m_live_state.gl_renderer_ptr->take_notes() )
            {
                log_line_vec.push_back(note);
            }
        }
        else
        if (is_draw == false)
        {
            if (is_d3d8)
            {
                m_live_state.d3d8_renderer_ptr->on_pre(in_api_func,
                                                       in_n_arguments,
                                                       in_arguments_ptr);
            }
            else
            {
                /* What a window cannot have is made in its way, and not by the call of the application. */
                if (m_live_state.d3d7_windowed_mode.on_pre(in_api_func,
                                                           in_n_arguments,
                                                           in_arguments_ptr) == false)
                {
                    *out_should_pass_through_ptr = false;
                }

                /* What is in pixels is made in the pixels of a larger back buffer, if it is made larger. */
                if (m_live_state.d3d7_renderer_ptr->on_pre(in_api_func,
                                                           in_n_arguments,
                                                           in_arguments_ptr) == false)
                {
                    *out_should_pass_through_ptr = false;
                }

                if (APIInterceptor::is_frame_boundary(in_api_func,
                                                      in_n_arguments,
                                                      in_arguments_ptr) )
                {
                    on_d3d7_frame_ending(in_api_func,
                                         in_arguments_ptr,
                                        &log_line_vec);
                }
            }
        }
        else
        {
            const auto draw_class = m_live_state.core_ptr->classify_draw(m_live_state.feeder.get_last_draw_signature_key(),
                                                                         m_live_state.feeder.get_last_draw_cover_key    () );

            /* A draw of Direct3D 7 or 8 is made by the renderer if it moves the vertices for the eye. */
            if (is_d3d8)
            {
                ScopedTickAdder draw_timing(&m_live_state.n_draw_ticks,
                                            true); /* in_is_enabled */

                *out_should_pass_through_ptr = m_live_state.d3d8_renderer_ptr->on_pre_draw(draw_class,
                                                                                           in_arguments_ptr);
            }
            else
            {
                ScopedTickAdder draw_timing(&m_live_state.n_draw_ticks,
                                            true); /* in_is_enabled */

                const auto get_chunk_func = [this](const uint32_t& in_chunk_id,
                                                   const void**    out_data_ptr_ptr,
                                                   uint32_t*       out_n_bytes_ptr)
                {
                    return m_live_state.feeder.get_last_call_chunk(in_chunk_id,
                                                                   out_data_ptr_ptr,
                                                                   out_n_bytes_ptr);
                };

                *out_should_pass_through_ptr = m_live_state.d3d7_renderer_ptr->on_pre_draw(in_api_func,
                                                                                           in_n_arguments,
                                                                                           in_arguments_ptr,
                                                                                           get_chunk_func,
                                                                                           m_live_state.feeder.get_viewport().width);
            }

            count_draw(draw_class);
        }

        if (is_present)
        {
            on_present_starting(in_arguments_ptr,
                               &right_surface_ptr,
                               &log_line_vec);
        }
    }

    for (const auto& log_line : log_line_vec)
    {
        m_log.write(log_line);
    }

    if (has_gl_images)
    {
        pass_gl_frame(gl_images);
    }

    if (is_present)
    {
        m_frame_passer_ptr->on_present(const_cast<void*>(in_arguments_ptr[0].get_ptr() ),
                                       nullptr, /* in_left_surface_ptr */
                                       right_surface_ptr);
    }
}

void Stereo::ConverterDLL::Converter::on_pre_callback(APIInterceptor::APIFunction                in_api_func,
                                                      uint32_t                                   in_n_arguments,
                                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                      void*                                      in_user_arg_ptr,
                                                      bool*                                      out_should_pass_through_ptr)
{
    /* The time the callbacks take is told in the log. The clock costs about as much as a call that is only noted, so it is read for one call of this many of a thread only; what
     * that one took is counted that many times. The number is a prime, so that it does not line up with a few calls that the application makes over and over (every one that is
     * timed would be the same call).
     */
    constexpr uint32_t N_CALLS_PER_TIMING = 61;

    static thread_local uint32_t n_calls_seen = 0;

    if (OwnCallScope::is_active() == false)
    {
        LARGE_INTEGER end_qpc   = {};
        LARGE_INTEGER start_qpc = {};
        const bool    is_timed  = (n_calls_seen % N_CALLS_PER_TIMING == 0);

        n_calls_seen++;

        if (is_timed)
        {
            ::QueryPerformanceCounter(&start_qpc);
        }

        static_cast<Converter*>(in_user_arg_ptr)->on_pre(in_api_func,
                                                         in_n_arguments,
                                                         in_arguments_ptr,
                                                         out_should_pass_through_ptr);

        if (is_timed)
        {
            ::QueryPerformanceCounter(&end_qpc);

            static_cast<Converter*>(in_user_arg_ptr)->m_live_state.n_callback_ticks += (end_qpc.QuadPart - start_qpc.QuadPart) * N_CALLS_PER_TIMING;
        }
    }
}

void Stereo::ConverterDLL::Converter::on_present_starting(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                          void**                                     out_right_surface_ptr_ptr,
                                                          std::vector<std::string>*                  out_log_line_vec_ptr)
{
    *out_right_surface_ptr_ptr = m_live_state.d3d8_renderer_ptr->get_back_buffer_twin_ptr(const_cast<void*>(in_arguments_ptr[0].get_ptr() ));

    /* The application renders to a window that cannot be seen; it may change its style. */
    m_windowed_mode.keep_windows_invisible();

    count_frame(out_log_line_vec_ptr);
}

bool Stereo::ConverterDLL::Converter::on_set_transform(const uint32_t&                            in_n_arguments,
                                                       const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t MATRIX_ARGUMENT_INDEX = 2;
    constexpr uint32_t STATE_ARGUMENT_INDEX  = 1;

    const void*           matrix_data_ptr = nullptr;
    std::array<float, 16> matrix;
    uint32_t              n_matrix_bytes  = 0;
    bool                  result          = false;

    if (in_n_arguments                               >  MATRIX_ARGUMENT_INDEX                 &&
        in_arguments_ptr[MATRIX_ARGUMENT_INDEX].type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        const bool has_chunk = APIInterceptor::get_data_chunk(in_arguments_ptr[MATRIX_ARGUMENT_INDEX].get_data_chunk_id(),
                                                             &matrix_data_ptr,
                                                             &n_matrix_bytes);

        if (has_chunk)
        {
            if (n_matrix_bytes == sizeof(matrix) )
            {
                ::memcpy(matrix.data(),
                         matrix_data_ptr,
                         sizeof(matrix) );

                if (in_arguments_ptr[STATE_ARGUMENT_INDEX].get_u32() == Interception::Hooks::D3D8::D3DTS_VIEW)
                {
                    m_live_state.core_ptr->on_view_matrix_set(matrix);
                }

                result = m_live_state.core_ptr->on_matrix_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM,
                                                               std::vector<APIInterceptor::APIFunctionArgument>(in_arguments_ptr,
                                                                                                                in_arguments_ptr + in_n_arguments),
                                                               matrix);

                if (result)
                {
                    m_live_state.n_kept_matrices++;
                }
            }
        }
    }

    return result;
}

void Stereo::ConverterDLL::Converter::pass_d3d7_frame(void* in_primary_surface_ptr,
                                                      void* in_back_buffer_ptr)
{
    constexpr uint32_t BACK_BUFFER_CAPS = 0x00000004; /* DDSCAPS_BACKBUFFER */

    const OwnCallScope                              own_call_scope;
    Interception::Hooks::D3D7::IDirectDrawSurface7* back_buffer_ptr = static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>( (in_back_buffer_ptr != nullptr) ? in_back_buffer_ptr
                                                                                                                                                                    : m_live_state.d3d7_windowed_mode.get_back_buffer_ptr() );
    bool                                            is_to_release   = false;

    /* The application renders to the back buffer of its window if it is made to; otherwise the chain it flips with has one. */
    if (back_buffer_ptr == nullptr)
    {
        Interception::Hooks::D3D7::DDSCAPS2 caps   = {};
        void** const                        vtable = APIInterceptor::get_vtable_ptr(in_primary_surface_ptr);

        caps.dwCaps = BACK_BUFFER_CAPS;

        is_to_release = SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE>(vtable,
                                                                                                                                       Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE)(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(in_primary_surface_ptr),
                                                                                                                                                                                                                      &caps,
                                                                                                                                                                                                                      &back_buffer_ptr) );

        if (is_to_release == false)
        {
            back_buffer_ptr = nullptr;
        }
    }

    if (back_buffer_ptr != nullptr)
    {
        void* const back_buffer_twin_ptr = m_live_state.d3d7_renderer_ptr->get_twin_ptr(back_buffer_ptr);

        /* A debugging aid: the images of the eyes of one frame, to look at. */
        if ( (m_live_state.dump_frame                           != 0                             &&
              m_live_state.n_frames                             == m_live_state.dump_frame)      ||
             (m_live_state.dump_every                           != 0                             &&
              m_live_state.n_frames                             >= m_live_state.dump_first_frame &&
              m_live_state.n_frames   % m_live_state.dump_every == 0) )
        {
            char temp_i8vec260[MAX_PATH] = {};

            if (::GetTempPathA(MAX_PATH,
                               temp_i8vec260) != 0)
            {
                const std::string filename   = std::string(temp_i8vec260)            +
                                               "apistereo_frame_"                    +
                                               std::to_string(m_live_state.n_frames) +
                                               ".bmp";
                const bool        is_written = D3D7HostMemoryExporter::write_debug_image(back_buffer_ptr,
                                                                                         back_buffer_twin_ptr,
                                                                                         filename);

                m_log.write((is_written) ? "A frame is written to a file"
                                         : "A frame cannot be written to a file");
            }
        }

        m_d3d7_frame_passer_ptr->on_present(nullptr, /* in_device_ptr */
                                            back_buffer_ptr,
                                            back_buffer_twin_ptr);

        if (is_to_release)
        {
            void** const back_buffer_vtable = APIInterceptor::get_vtable_ptr(back_buffer_ptr);

            APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_RELEASE>(back_buffer_vtable,
                                                                                                       Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_RELEASE)(back_buffer_ptr);
        }
    }
}

void Stereo::ConverterDLL::Converter::pass_gl_frame(const GLFrameImages& in_images)
{
    /* A debugging aid: the images of the eyes of one frame, to look at (the same switches as for Direct3D 7). */
    if ( (m_live_state.dump_frame                           != 0                             &&
          m_live_state.n_frames                             == m_live_state.dump_frame)      ||
         (m_live_state.dump_every                           != 0                             &&
          m_live_state.n_frames                             >= m_live_state.dump_first_frame &&
          m_live_state.n_frames   % m_live_state.dump_every == 0) )
    {
        char temp_i8vec260[MAX_PATH] = {};

        if (::GetTempPathA(MAX_PATH,
                           temp_i8vec260) != 0)
        {
            const std::string filename   = std::string(temp_i8vec260)            +
                                           "apistereo_frame_"                    +
                                           std::to_string(m_live_state.n_frames) +
                                           ".bmp";
            const bool        is_written = GLHostMemoryExporter::write_debug_image(in_images,
                                                                                   filename);

            m_log.write((is_written) ? "A frame is written to a file"
                                     : "A frame cannot be written to a file");
        }
    }

    m_gl_frame_passer_ptr->on_present(nullptr,
                                      const_cast<GLFrameImages*>(&in_images),
                                      const_cast<GLFrameImages*>(&in_images) );
}

void Stereo::ConverterDLL::Converter::read_channel_settings(std::vector<std::string>* out_log_line_vec_ptr)
{
    float convergence_distance = 0.0f;
    float depth_strength       = 0.0f;
    float eye_separation       = 0.0f;

    const bool has_eye_separation = m_channel_ptr->get_eye_separation(&eye_separation);

    if (has_eye_separation)
    {
        if (eye_separation != m_live_state.core_ptr->get_eye_separation() )
        {
            m_live_state.core_ptr->set_eye_separation(eye_separation);

            out_log_line_vec_ptr->push_back("The eye separation is now "   +
                                            std::to_string(eye_separation) );
        }
    }

    const bool has_depth_strength = m_channel_ptr->get_depth_strength(&depth_strength);

    if (has_depth_strength)
    {
        if (depth_strength != m_live_state.core_ptr->get_depth_strength() )
        {
            m_live_state.core_ptr->set_depth_strength(depth_strength);

            out_log_line_vec_ptr->push_back("The strength of the 3D is now " +
                                            std::to_string(depth_strength) );
        }
    }

    float ipd_scale = 1.0f;

    const bool has_ipd_scale = m_channel_ptr->get_ipd_scale(&ipd_scale);

    if (has_ipd_scale)
    {
        if (ipd_scale != m_live_state.core_ptr->get_ipd_scale() )
        {
            m_live_state.core_ptr->set_ipd_scale(ipd_scale);

            out_log_line_vec_ptr->push_back("The scale of the eyes of the person is now " +
                                            std::to_string(ipd_scale) );
        }
    }

    const bool is_overlay_pushed = m_channel_ptr->get_is_overlay_pushed();

    if (m_live_state.is_overlay_push_forced == false)
    {
        const bool was_overlay_pushed = m_live_state.core_ptr->get_is_overlay_pushed();

        if (is_overlay_pushed != was_overlay_pushed)
        {
            m_live_state.core_ptr->set_is_overlay_pushed(is_overlay_pushed);

            out_log_line_vec_ptr->push_back((is_overlay_pushed) ? "What the recipe leaves alone is now pushed toward the viewer"
                                                                : "What the recipe leaves alone is no longer pushed toward the viewer");
        }
    }

    const bool has_convergence_distance = m_channel_ptr->get_convergence_distance(&convergence_distance);

    if (has_convergence_distance)
    {
        if (convergence_distance != m_live_state.core_ptr->get_convergence_distance() )
        {
            m_live_state.core_ptr->set_convergence_distance(convergence_distance);

            out_log_line_vec_ptr->push_back("The convergence distance is now " +
                                            std::to_string(convergence_distance) );
        }
    }
}

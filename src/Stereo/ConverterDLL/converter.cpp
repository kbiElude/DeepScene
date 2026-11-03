/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/RecipeLoader/recipe_loader.h"
#include "Capture/APIDumpLoader/api_info.h"
#include "Common/frame_boundary.h"
#include "Interception/Hooks/D3D8/types.h"
#include "Stereo/Session/session_environment.h"
#include "converter.h"
#include "own_call_scope.h"
#include <array>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>

Stereo::ConverterDLL::Converter::Converter(const Stereo::Session::SessionInfo&              in_session_info,
                                           const Analysis::RecipeLoader::Recipe&            in_recipe,
                                           std::unique_ptr<Stereo::Transport::FrameChannel> in_channel_ptr)
    :m_channel_ptr(std::move(in_channel_ptr) ),
     m_log        (in_session_info)
{
    /* A converter that finds that it cannot convert the application right ends it, and says why in the log. */
    m_live_state.core_ptr.reset(new Stereo::Core::StereoCore(in_recipe,
                                                             [this](const std::string& in_message)
                                                             {
                                                                 m_log.write_failure("The application cannot be converted: " + in_message);

                                                                 ::TerminateProcess(::GetCurrentProcess(),
                                                                                    Stereo::Session::SessionEnvironment::get_converter_failed_exit_code() );
                                                             }) );

    /* The calls that the front end of Direct3D 8 follows, before and after. A frame is passed on before the present, since what the
     * back buffer holds after a present is not defined.
     */
    for (uint32_t n_api_func = 0;
                  n_api_func < APIInterceptor::APIFUNCTION_COUNT;
                ++n_api_func)
    {
        const auto function = static_cast<APIInterceptor::APIFunction>(n_api_func);

        /* The rest of the converter (the eye renderer, the scaler, the exporter) is for Direct3D 8: of the calls of Direct3D 7 it only
         * follows what the application does (see on_d3d7_pre()), until the ones of Direct3D 7 are made.
         */
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

    /* A way to set how far apart the eyes are for a run without the program which shows the images, which says it otherwise. */
    {
        char                eye_separation_i8vec32[32] = {};
        const unsigned long n_chars                    = ::GetEnvironmentVariableA("APISTEREO_EYE_SEPARATION",
                                                                                   eye_separation_i8vec32,
                                                                                   static_cast<DWORD>(sizeof(eye_separation_i8vec32) ));

        if (n_chars > 0                                        &&
            n_chars < sizeof(eye_separation_i8vec32) )
        {
            m_live_state.core_ptr->set_eye_separation(static_cast<float>(::atof(eye_separation_i8vec32) ));

            m_log.write(std::string("The eye separation is ") + eye_separation_i8vec32 + ", from APISTEREO_EYE_SEPARATION");
        }
    }

    m_live_state.d3d7_renderer_ptr.reset(new D3D7EyeRenderer(m_live_state.core_ptr.get() ));
    m_live_state.renderer_ptr.reset(new D3D8EyeRenderer(m_live_state.core_ptr.get                               () ));
    m_frame_passer_ptr.reset       (new FramePasser    (std::unique_ptr<Stereo::Transport::IFrameExporter>(new D3D8HostMemoryExporter(m_channel_ptr.get() )),
                                                        m_log) );

    {
        /* Said by the program which shows the images, before it starts the application: the device is made with it. */
        uint32_t target_height = 0;
        uint32_t target_width  = 0;

        m_channel_ptr->get_target_eye_size(&target_width,
                                           &target_height);

        m_live_state.scaler_ptr.reset(new D3D8ResolutionScaler(target_width,
                                                               target_height,
                                                               m_live_state.core_ptr.get(),
                                                               [this]()
                                                               {
                                                                   return m_live_state.renderer_ptr->get_render_target_ptr();
                                                               },
                                                               [this](const std::string& in_message)
                                                               {
                                                                   m_log.write(in_message);
                                                               }) );

        /* The analysis sees the application as it asked to be, or the signatures of its draws would depend on the scale. */
        m_live_state.feeder.set_chunk_func([this](const APIInterceptor::APIFunction& in_function,
                                                  std::vector<uint8_t>*              inout_chunk_ptr)
                                           {
                                               m_live_state.scaler_ptr->restore_chunk(in_function,
                                                                                      inout_chunk_ptr);
                                           });
    }

    m_log.write("The converter is loaded");
}

Stereo::ConverterDLL::Converter::~Converter()
{
    m_log.write("The converter is removed");
}

void Stereo::ConverterDLL::Converter::count_draw(const Analysis::RecipeLoader::DrawClass& in_draw_class)
{
    switch (in_draw_class)
    {
        case Analysis::RecipeLoader::DrawClass::SKIPPED:
        {
            m_live_state.n_skipped_draws++;

            break;
        }

        case Analysis::RecipeLoader::DrawClass::UNAFFECTED:
        {
            m_live_state.n_unaffected_draws++;

            break;
        }

        case Analysis::RecipeLoader::DrawClass::AFFECTED:
        default:
        {
            m_live_state.n_affected_draws++;

            break;
        }
    }
}

std::unique_ptr<Stereo::ConverterDLL::Converter> Stereo::ConverterDLL::Converter::create_ptr()
{
    std::string                  error;
    std::unique_ptr<Converter>   result_ptr;
    Stereo::Session::SessionInfo session_info;

    if (Stereo::Session::SessionEnvironment::read(&session_info) )
    {
        Analysis::RecipeLoader::Recipe recipe;
        auto                           channel_ptr = Stereo::Transport::FrameChannel::open_ptr(session_info.session_id,
                                                                                              &error);

        if (channel_ptr != nullptr)
        {
            const bool is_recipe_loaded = Analysis::RecipeLoader::RecipeLoader::load(session_info.recipe_path,
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
           " matrices kept";
}

void Stereo::ConverterDLL::Converter::on_d3d7_frame_ending(std::vector<std::string>* out_log_line_vec_ptr)
{
    constexpr uint64_t N_FRAMES_BETWEEN_STATISTICS = 600;

    m_live_state.n_frames++;

    if (m_live_state.n_frames % N_FRAMES_BETWEEN_STATISTICS == 0)
    {
        out_log_line_vec_ptr->push_back(make_statistics_line() );
    }

    read_channel_settings(out_log_line_vec_ptr);
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
        std::lock_guard<std::mutex> lock(this_ptr->m_live_state.mutex);

        uint64_t shape_key = 0;

        this_ptr->m_live_state.feeder.on_post(in_api_func,
                                              in_n_arguments_out,
                                              in_arguments_out_ptr,
                                              in_returned_value_ptr);

        /* What the frame that ended was made of tells which scene it is; the draws that follow are converged for it. */
        if (this_ptr->m_live_state.feeder.take_completed_frame_shape_key(&shape_key) )
        {
            this_ptr->m_live_state.core_ptr->on_frame_completed(shape_key);
        }

        /* The eye renderer is for Direct3D 8; the calls of Direct3D 7 are only followed. */
        if (Capture::APIDumpLoader::get_api_group(in_api_func) == Capture::APIDumpLoader::APIGroup::D3D8)
        {
            this_ptr->m_live_state.renderer_ptr->on_post(in_api_func,
                                                         in_n_arguments_out,
                                                         in_arguments_out_ptr);
        }
    }

    /* The frame is presented: the application makes the next one when the screen wants it, which is what the program that shows the
     * frames says (if it does). Outside the lock of the converter, since it is as long as a frame.
     */
    if (OwnCallScope::is_active() == false &&
        in_api_func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT)
    {
        this_ptr->m_frame_passer_ptr->wait_for_pace();
    }
}

void Stereo::ConverterDLL::Converter::on_pre(const APIInterceptor::APIFunction&         in_api_func,
                                             const uint32_t&                            in_n_arguments,
                                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                             bool*                                      out_should_pass_through_ptr)
{
    const bool               is_present        = (in_api_func    == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT &&
                                                  in_n_arguments >  0);
    std::vector<std::string> log_line_vec;
    void*                    right_surface_ptr = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_live_state.mutex);

        const bool is_d3d8 = (Capture::APIDumpLoader::get_api_group(in_api_func) == Capture::APIDumpLoader::APIGroup::D3D8);
        const bool is_draw = m_live_state.feeder.on_pre(in_api_func,
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

        if (is_draw == false)
        {
            if (is_d3d8)
            {
                m_live_state.renderer_ptr->on_pre(in_api_func,
                                                  in_n_arguments,
                                                  in_arguments_ptr);
            }
            else
            if (APIInterceptor::is_frame_boundary(in_api_func,
                                                  in_n_arguments,
                                                  in_arguments_ptr) )
            {
                on_d3d7_frame_ending(&log_line_vec);
            }
        }
        else
        {
            const auto draw_class = m_live_state.core_ptr->classify_draw(m_live_state.feeder.get_last_draw_signature_key(),
                                                                                      m_live_state.feeder.get_last_draw_cover_key() );

            /* A draw of Direct3D 7 is made by the renderer if it moves the vertices for the eye. */
            if (is_d3d8)
            {
                *out_should_pass_through_ptr = m_live_state.renderer_ptr->on_pre_draw(draw_class,
                                                                                      in_arguments_ptr);
            }
            else
            {
                *out_should_pass_through_ptr = m_live_state.d3d7_renderer_ptr->on_pre_draw(in_api_func,
                                                                                           in_n_arguments,
                                                                                           in_arguments_ptr,
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

    if (is_present)
    {
        m_frame_passer_ptr->on_present(const_cast<void*>(in_arguments_ptr[0].get_ptr() ),
                                       right_surface_ptr);
    }
}

void Stereo::ConverterDLL::Converter::on_pre_callback(APIInterceptor::APIFunction                in_api_func,
                                                      uint32_t                                   in_n_arguments,
                                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                      void*                                      in_user_arg_ptr,
                                                      bool*                                      out_should_pass_through_ptr)
{
    if (OwnCallScope::is_active() == false)
    {
        static_cast<Converter*>(in_user_arg_ptr)->on_pre(in_api_func,
                                                         in_n_arguments,
                                                         in_arguments_ptr,
                                                         out_should_pass_through_ptr);
    }
}

void Stereo::ConverterDLL::Converter::on_present_starting(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                          void**                                     out_right_surface_ptr_ptr,
                                                          std::vector<std::string>*                  out_log_line_vec_ptr)
{
    constexpr uint64_t N_FRAMES_BETWEEN_STATISTICS = 600;

    *out_right_surface_ptr_ptr = m_live_state.renderer_ptr->get_back_buffer_twin_ptr(const_cast<void*>(in_arguments_ptr[0].get_ptr() ));

    /* The application renders to a window that cannot be seen; it may change its style. */
    m_windowed_mode.keep_windows_invisible();

    m_live_state.n_frames++;

    if (m_live_state.n_frames % N_FRAMES_BETWEEN_STATISTICS == 0)
    {
        out_log_line_vec_ptr->push_back(make_statistics_line() );
    }

    read_channel_settings(out_log_line_vec_ptr);
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

            out_log_line_vec_ptr->push_back("The eye separation is now " + std::to_string(eye_separation) );
        }
    }

    const bool has_depth_strength = m_channel_ptr->get_depth_strength(&depth_strength);

    if (has_depth_strength)
    {
        if (depth_strength != m_live_state.core_ptr->get_depth_strength() )
        {
            m_live_state.core_ptr->set_depth_strength(depth_strength);

            out_log_line_vec_ptr->push_back("The strength of the 3D is now " + std::to_string(depth_strength) );
        }
    }

    float ipd_scale = 1.0f;

    const bool has_ipd_scale = m_channel_ptr->get_ipd_scale(&ipd_scale);

    if (has_ipd_scale)
    {
        if (ipd_scale != m_live_state.core_ptr->get_ipd_scale() )
        {
            m_live_state.core_ptr->set_ipd_scale(ipd_scale);

            out_log_line_vec_ptr->push_back("The scale of the eyes of the person is now " + std::to_string(ipd_scale) );
        }
    }

    const bool is_overlay_pushed = m_channel_ptr->get_is_overlay_pushed();

    if (is_overlay_pushed != m_live_state.core_ptr->get_is_overlay_pushed() )
    {
        m_live_state.core_ptr->set_is_overlay_pushed(is_overlay_pushed);

        out_log_line_vec_ptr->push_back((is_overlay_pushed) ? "What the recipe leaves alone is now pushed toward the viewer"
                                                            : "What the recipe leaves alone is no longer pushed toward the viewer");
    }

    const bool has_convergence_distance = m_channel_ptr->get_convergence_distance(&convergence_distance);

    if (has_convergence_distance)
    {
        if (convergence_distance != m_live_state.core_ptr->get_convergence_distance() )
        {
            m_live_state.core_ptr->set_convergence_distance(convergence_distance);

            out_log_line_vec_ptr->push_back("The convergence distance is now " + std::to_string(convergence_distance) );
        }
    }
}

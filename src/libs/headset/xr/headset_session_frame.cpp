/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "headset_session_impl.h"
#include <chrono>
#include <math.h>
#include <stdio.h>

/* How far the screen is from the head when it is put: where eyes of a headset focus is about this. */
constexpr float SCREEN_DISTANCE = 2.0f;

void Headset::XR::HeadsetSession::Impl::end_frame(const XrTime&                              in_time,
                                                  const uint32_t&                            in_n_layers,
                                                  const XrCompositionLayerBaseHeader* const* in_layer_ptr_array)
{
    XrFrameEndInfo end_info   = {XR_TYPE_FRAME_END_INFO};
    XrResult       end_result = XR_SUCCESS;

    end_info.displayTime          = in_time;
    end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end_info.layerCount           = in_n_layers;
    end_info.layers               = in_layer_ptr_array;

    end_result = ::xrEndFrame(session,
                             &end_info);

    if (XR_FAILED(end_result) )
    {
        log_failure("xrEndFrame",
                    end_result);
    }
}

bool Headset::XR::HeadsetSession::Impl::locate_head(const XrTime& in_time)
{
    XrSpaceLocation location = {XR_TYPE_SPACE_LOCATION};
    bool            result   = false;

    const XrResult locate_result = ::xrLocateSpace(view_space,
                                                   local_space,
                                                   in_time,
                                                  &location);

    if (XR_SUCCEEDED(locate_result) )
    {
        if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)    != 0 &&
            (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0)
        {
            screen_position_fp32vec3[0] = location.pose.position.x;
            screen_position_fp32vec3[1] = location.pose.position.y;
            screen_position_fp32vec3[2] = location.pose.position.z;
            screen_yaw                  = ScreenGeometry::get_yaw(location.pose.orientation.x,
                                                                  location.pose.orientation.y,
                                                                  location.pose.orientation.z,
                                                                  location.pose.orientation.w);

            result = true;
        }
    }

    return result;
}

void Headset::XR::HeadsetSession::Impl::log_failure(const char*     in_what_ptr,
                                                    const XrResult& in_result)
{
    bool is_new = false;

    {
        std::lock_guard<std::mutex> lock(log_mutex);

        is_new = logged_result_set.insert(static_cast<int>(in_result) ).second;
    }

    if (is_new)
    {
        add_log(std::string(in_what_ptr) + " failed (" + get_result_text(in_result) + ")");
    }
}

void Headset::XR::HeadsetSession::Impl::log_screen(const float& in_aspect_ratio)
{
    constexpr float TO_DEGREES = 180.0f / 3.14159265f;

    const auto screen = ScreenGeometry::fit_cylinder(field_of_view,
                                                     in_aspect_ratio,
                                                     SCREEN_DISTANCE,
                                                     1.0f); /* in_scale */

    add_log(std::string("The screen is a ")                                                     +
            ((is_cylinder_supported) ? "cylinder" : "quad")                                     +
            " "                                                                                 +
            std::to_string(SCREEN_DISTANCE)                                                     +
            " m away, "                                                                         +
            std::to_string(screen.central_angle * TO_DEGREES)                                   +
            " degrees wide for a view of "                                                      +
            std::to_string((field_of_view.angle_right - field_of_view.angle_left) * TO_DEGREES) +
            " x "                                                                               +
            std::to_string((field_of_view.angle_up - field_of_view.angle_down) * TO_DEGREES)    +
            " degrees, for an image of "                                                        +
            std::to_string(swapchain_width)                                                     +
            " x "                                                                               +
            std::to_string(swapchain_height)                                                    +
            " per eye");

    has_logged_screen = true;
}

void Headset::XR::HeadsetSession::Impl::make_layer(const uint32_t& in_n_eye,
                                                   const XrPosef&  in_pose,
                                                   const float&    in_aspect_ratio)
{
    const XrEyeVisibility eye_visibility = (in_n_eye == 0) ? XR_EYE_VISIBILITY_LEFT
                                                           : XR_EYE_VISIBILITY_RIGHT;
    XrSwapchainSubImage   sub_image      = {};

    sub_image.imageRect.extent.height = static_cast<int32_t>(swapchain_height);
    sub_image.imageRect.extent.width  = static_cast<int32_t>(swapchain_width);
    sub_image.swapchain               = eye_array.at        (in_n_eye).swapchain;

    if (is_cylinder_supported)
    {
        const CylinderScreen           screen = ScreenGeometry::fit_cylinder(field_of_view,
                                                                             in_aspect_ratio,
                                                                             SCREEN_DISTANCE,
                                                                             1.0f); /* in_scale */
        XrCompositionLayerCylinderKHR& layer  = cylinder_layer_array[in_n_eye];

        layer               = {XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR};
        layer.aspectRatio   = screen.aspect_ratio;
        layer.centralAngle  = screen.central_angle;
        layer.eyeVisibility = eye_visibility;
        layer.pose          = in_pose;
        layer.radius        = screen.radius;
        layer.space         = local_space;
        layer.subImage      = sub_image;
    }
    else
    {
        const QuadScreen        screen = ScreenGeometry::fit_quad(field_of_view,
                                                                  in_aspect_ratio,
                                                                  SCREEN_DISTANCE,
                                                                  1.0f); /* in_scale */
        XrCompositionLayerQuad& layer  = quad_layer_array[in_n_eye];

        layer               = {XR_TYPE_COMPOSITION_LAYER_QUAD};
        layer.eyeVisibility = eye_visibility;
        layer.pose          = in_pose;
        layer.size.height   = screen.height;
        layer.size.width    = screen.width;
        layer.space         = local_space;
        layer.subImage      = sub_image;

        /* The quad is in front of the head, not around it. */
        layer.pose.position.x -= ::sinf(screen_yaw) * SCREEN_DISTANCE;
        layer.pose.position.z -= ::cosf(screen_yaw) * SCREEN_DISTANCE;
    }
}

uint32_t Headset::XR::HeadsetSession::Impl::make_layers(const XrTime& in_time)
{
    uint32_t result = 0;

    if (eye_array.at(0).has_image == false ||
        eye_array.at(1).has_image == false)
    {
        goto end;
    }

    update_field_of_view(in_time);

    if (has_field_of_view == false ||
        is_placed         == false)
    {
        goto end;
    }

    {
        const float aspect_ratio = static_cast<float>(swapchain_width) / static_cast<float>(swapchain_height);
        XrPosef     pose         = {};

        pose.orientation.w = ::cosf(screen_yaw / 2.0f);
        pose.orientation.y = ::sinf(screen_yaw / 2.0f);
        pose.position.x    = screen_position_fp32vec3[0];
        pose.position.y    = screen_position_fp32vec3[1];
        pose.position.z    = screen_position_fp32vec3[2];

        for (uint32_t n_eye = 0;
                      n_eye < 2;
                    ++n_eye)
        {
            make_layer(n_eye,
                       pose,
                       aspect_ratio);
        }

        if (has_logged_screen == false)
        {
            log_screen(aspect_ratio);
        }

        result = 2;
    }

end:
    return result;
}

void Headset::XR::HeadsetSession::Impl::on_session_state_changed(const XrSessionState& in_state)
{
    add_log(std::string   ("The session is ") +
            get_state_name(in_state) );

    switch (in_state)
    {
        case XR_SESSION_STATE_READY:
        {
            XrSessionBeginInfo begin_info = {XR_TYPE_SESSION_BEGIN_INFO};

            begin_info.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

            if (XR_SUCCEEDED(::xrBeginSession(session,
                                             &begin_info) ))
            {
                std::string error;
                uint32_t    eye_height = 0;
                uint32_t    eye_width  = 0;

                std::lock_guard<std::mutex> lock(mutex);

                is_running = true;

                /* Black is shown from the start, until the frames of the application come (they make the swap chains again if they are of another
                 * size): with nothing to show, the runtime shows what it likes, and it flickers.
                 */
                if (read_recommended_eye_size(&eye_width,
                                              &eye_height) )
                {
                    if (make_swapchains(eye_width,
                                        eye_height,
                                        &error) == false)
                    {
                        add_log("The black screen cannot be shown before the first frame: " + error);
                    }
                }
            }

            break;
        }

        case XR_SESSION_STATE_STOPPING:
        {
            {
                std::lock_guard<std::mutex> lock(mutex);

                is_running = false;
            }

            ::xrEndSession(session);

            break;
        }

        case XR_SESSION_STATE_EXITING:
        case XR_SESSION_STATE_LOSS_PENDING:
        {
            is_exit_requested = true;

            break;
        }

        default:
        {
            break;
        }
    }
}

void Headset::XR::HeadsetSession::Impl::poll_events()
{
    XrEventDataBuffer event = {XR_TYPE_EVENT_DATA_BUFFER};

    while (::xrPollEvent(instance,
                        &event) == XR_SUCCESS)
    {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
        {
            on_session_state_changed(reinterpret_cast<const XrEventDataSessionStateChanged*>(&event)->state);
        }
        else
        if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
        {
            is_exit_requested = true;
        }

        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

bool Headset::XR::HeadsetSession::Impl::read_recenter()
{
    XrActiveActionSet active_set = {};
    bool              result     = false;
    XrActionsSyncInfo sync_info  = {XR_TYPE_ACTIONS_SYNC_INFO};

    active_set.actionSet = action_set;

    sync_info.activeActionSets      = &active_set;
    sync_info.countActiveActionSets = 1;

    if (XR_SUCCEEDED(::xrSyncActions(session,
                                    &sync_info) ))
    {
        XrActionStateGetInfo get_info = {XR_TYPE_ACTION_STATE_GET_INFO};
        XrActionStateBoolean state    = {XR_TYPE_ACTION_STATE_BOOLEAN};

        get_info.action = action;

        if (XR_SUCCEEDED(::xrGetActionStateBoolean(session,
                                                  &get_info,
                                                  &state) ))
        {
            result = (state.isActive             == XR_TRUE &&
                      state.changedSinceLastSync == XR_TRUE &&
                      state.currentState         == XR_TRUE);
        }
    }

    return result;
}

bool Headset::XR::HeadsetSession::run_frame(std::string* out_error_ptr)
{
    constexpr DWORD  IDLE_N_MILLISECONDS          = 10;
    constexpr int    IMAGE_WAIT_N_MICROSECONDS    = 6000; /* the frame has this much room before the compositor wants it */
    constexpr double LATE_PERIODS                 = 1.5;
    constexpr double REPORT_PERIOD_N_MILLISECONDS = 1000.0;
    constexpr double N_NANOSECONDS_PER_MILLISECOND = 1000000.0;

    XrFrameState frame_state = {XR_TYPE_FRAME_STATE};
    bool         result      = true;

    (void) out_error_ptr;

    m_impl_ptr->poll_events();

    if (m_impl_ptr->is_exit_requested)
    {
        result = false;

        goto end;
    }

    {
        bool is_running = false;

        {
            std::lock_guard<std::mutex> lock(m_impl_ptr->mutex);

            is_running = m_impl_ptr->is_running;
        }

        /* A frame is waited for only in a running session: until the runtime says that it is ready, there is nothing to wait for. */
        if (is_running == false)
        {
            ::Sleep(IDLE_N_MILLISECONDS);

            goto end;
        }
    }

    {
        const double   wait_start_ms = Utils::Statistics::SeriesStats::get_now();
        const XrResult wait_result   = ::xrWaitFrame(m_impl_ptr->session,
                                                     nullptr,
                                                    &frame_state);
        const double   wait_end_ms   = Utils::Statistics::SeriesStats::get_now();

        if (XR_SUCCEEDED(wait_result) )
        {
            auto& timing = m_impl_ptr->timing;

            timing.period_ms = static_cast<float>(static_cast<double>(frame_state.predictedDisplayPeriod) / N_NANOSECONDS_PER_MILLISECOND);

            timing.frame_interval.set_late_threshold(LATE_PERIODS * static_cast<double>(timing.period_ms) );
            timing.frame_interval.add_interval      (wait_end_ms);
            timing.wait_cost.add                    (wait_end_ms - wait_start_ms);

            /* The compositor wants a frame: the application is to make the one that it shows next. */
            if (m_impl_ptr->frame_func)
            {
                m_impl_ptr->frame_func(static_cast<double>(timing.period_ms) );
            }
        }

        if (XR_FAILED(wait_result) )
        {
            m_impl_ptr->log_failure("xrWaitFrame",
                                    wait_result);

            ::Sleep(IDLE_N_MILLISECONDS);

            goto end;
        }
    }

    {
        XrFrameBeginInfo begin_info = {XR_TYPE_FRAME_BEGIN_INFO};

        const XrResult begin_result = ::xrBeginFrame(m_impl_ptr->session,
                                                    &begin_info);

        if (XR_FAILED(begin_result) )
        {
            m_impl_ptr->log_failure("xrBeginFrame",
                                    begin_result);

            ::Sleep(IDLE_N_MILLISECONDS);

            goto end;
        }
    }

    {
        const XrCompositionLayerBaseHeader* layer_ptr_array[2] = {};
        uint32_t                            n_layers           = 0;

        /* The screen is put in front of the head the first time the head is known, and when the user says so. */
        const bool is_recenter = m_impl_ptr->read_recenter();

        if (is_recenter                     ||
            m_impl_ptr->is_placed == false)
        {
            m_impl_ptr->is_placed = m_impl_ptr->locate_head(frame_state.predictedDisplayTime);

            if (is_recenter                 &&
                m_impl_ptr->is_placed)
            {
                m_impl_ptr->has_logged_screen = false;

                m_impl_ptr->add_log("The screen is put where the user looks");
            }
        }

        if (frame_state.shouldRender == XR_TRUE)
        {
            auto&        timing        = m_impl_ptr->timing;
            const double lock_start_ms = Utils::Statistics::SeriesStats::get_now();

            std::unique_lock<std::mutex> lock(m_impl_ptr->mutex);

            timing.lock_wait.add(Utils::Statistics::SeriesStats::get_now() - lock_start_ms);

            /* The images that the application's frame made are not always there when the compositor asks: a copy that is late by a few
             * milliseconds would show the frame before again, and the next one in its place a frame later. The frame is not ended for a
             * little while for them, which it has room for; if they do not come, the frame before is shown.
             */
            if (m_impl_ptr->image_serial == timing.last_serial)
            {
                const double image_wait_start_ms = Utils::Statistics::SeriesStats::get_now();

                m_impl_ptr->image_condition.wait_for(lock,
                                                     std::chrono::microseconds(IMAGE_WAIT_N_MICROSECONDS),
                                                     [this_ptr = m_impl_ptr.get(), &timing]()
                                                     {
                                                         return (this_ptr->image_serial != timing.last_serial ||
                                                                 this_ptr->is_exit_requested);
                                                     });

                timing.image_wait.add(Utils::Statistics::SeriesStats::get_now() - image_wait_start_ms);
            }

            n_layers = m_impl_ptr->make_layers(frame_state.predictedDisplayTime);

            /* Whether the images of the eyes are new to the compositor, or the ones that it showed already. */
            if (n_layers == 0)
            {
                ++timing.n_empty;
            }
            else
            if (m_impl_ptr->image_serial == timing.last_serial)
            {
                ++timing.n_repeated;
            }
            else
            {
                ++timing.n_new;

                timing.last_serial = m_impl_ptr->image_serial;
            }

            for (uint32_t n_layer = 0;
                          n_layer < n_layers;
                        ++n_layer)
            {
                layer_ptr_array[n_layer] = (m_impl_ptr->is_cylinder_supported) ? reinterpret_cast<const XrCompositionLayerBaseHeader*>(&m_impl_ptr->cylinder_layer_array[n_layer])
                                                                               : reinterpret_cast<const XrCompositionLayerBaseHeader*>(&m_impl_ptr->quad_layer_array    [n_layer]);
            }

            const double end_start_ms = Utils::Statistics::SeriesStats::get_now();

            m_impl_ptr->end_frame(frame_state.predictedDisplayTime,
                                  n_layers,
                                  layer_ptr_array);

            timing.end_cost.add(Utils::Statistics::SeriesStats::get_now() - end_start_ms);
        }
        else
        {
            ++m_impl_ptr->timing.n_skipped;

            m_impl_ptr->end_frame(frame_state.predictedDisplayTime,
                                  0, /* in_n_layers */
                                  layer_ptr_array);
        }
    }

    {
        auto&        timing = m_impl_ptr->timing;
        const double now_ms = Utils::Statistics::SeriesStats::get_now();

        if (Utils::Statistics::SeriesStats::take_period(&timing.last_report,
                                                        now_ms,
                                                        REPORT_PERIOD_N_MILLISECONDS) )
        {
            char counts_i8vec96[96];

            ::snprintf(counts_i8vec96,
                       sizeof(counts_i8vec96),
                       "images new=%llu repeated=%llu empty=%llu skipped=%llu",
                       static_cast<unsigned long long>(timing.n_new),
                       static_cast<unsigned long long>(timing.n_repeated),
                       static_cast<unsigned long long>(timing.n_empty),
                       static_cast<unsigned long long>(timing.n_skipped) );

            timing.n_empty    = 0;
            timing.n_new      = 0;
            timing.n_repeated = 0;
            timing.n_skipped  = 0;

            m_impl_ptr->add_log("Frames of the compositor (ms, the period is "                       +
                                std::to_string(timing.period_ms)                                   +
                                "): "                                                              +
                                timing.frame_interval.take_text("frames")                          +
                                "; "                                                               +
                                timing.wait_cost.take_text    ("wait for frame")                  +
                                "; "                                                               +
                                timing.lock_wait.take_text    ("wait for the lock")               +
                                "; "                                                               +
                                timing.image_wait.take_text   ("wait for new images")             +
                                "; "                                                               +
                                timing.end_cost.take_text     ("end of frame")                    +
                                "; "                                                               +
                                counts_i8vec96);
        }
    }

end:
    return result;
}

void Headset::XR::HeadsetSession::Impl::update_field_of_view(const XrTime& in_time)
{
    XrViewLocateInfo locate_info = {XR_TYPE_VIEW_LOCATE_INFO};
    uint32_t         n_views     = 0;
    XrView           view_array[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    XrViewState      view_state    = {XR_TYPE_VIEW_STATE};

    locate_info.displayTime           = in_time;
    locate_info.space                 = local_space;
    locate_info.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

    const XrResult locate_result = ::xrLocateViews(session,
                                                  &locate_info,
                                                  &view_state,
                                                   2,
                                                  &n_views,
                                                   view_array);

    if (XR_SUCCEEDED(locate_result) )
    {
        if (n_views == 2)
        {
            /* The distance between the eyes: between the places the runtime puts the two views, which it changes with the adjustment of the
             * headset. Smoothed, since it is a measure of a head that moves.
             */
            if ((view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0)
            {
                constexpr float SMOOTHING_PER_FRAME = 0.05f;

                const float x_distance = view_array[1].pose.position.x - view_array[0].pose.position.x;
                const float y_distance = view_array[1].pose.position.y - view_array[0].pose.position.y;
                const float z_distance = view_array[1].pose.position.z - view_array[0].pose.position.z;
                const float distance   = ::sqrtf(x_distance * x_distance + y_distance * y_distance + z_distance * z_distance);
                const float previous   = ipd;

                if (distance > 0.0f)
                {
                    ipd = (previous > 0.0f) ? previous + SMOOTHING_PER_FRAME * (distance - previous)
                                            : distance;
                }
            }

            Headset::XR::FieldOfView first;
            Headset::XR::FieldOfView second;

            first.angle_down  = view_array[0].fov.angleDown;
            first.angle_left  = view_array[0].fov.angleLeft;
            first.angle_right = view_array[0].fov.angleRight;
            first.angle_up    = view_array[0].fov.angleUp;

            second.angle_down  = view_array[1].fov.angleDown;
            second.angle_left  = view_array[1].fov.angleLeft;
            second.angle_right = view_array[1].fov.angleRight;
            second.angle_up    = view_array[1].fov.angleUp;

            field_of_view     = ScreenGeometry::get_common_field_of_view(first,
                                                                         second);
            has_field_of_view = true;
        }
    }
}

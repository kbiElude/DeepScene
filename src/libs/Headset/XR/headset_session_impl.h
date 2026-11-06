/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What a HeadsetSession keeps of the session, shared by the files that implement it: headset_session.cpp (the interface),
 * headset_session_setup.cpp (the runtime, the device, the session), headset_session_swapchains.cpp (the images of the eyes) and
 * headset_session_frame.cpp (the frames of the compositor).
 */
#ifndef HEADSET_XR_HEADSET_SESSION_IMPL_H
#define HEADSET_XR_HEADSET_SESSION_IMPL_H

#include "Headset/D3D12/d3d12_texture_clearer.h"
#include "Stereo/Transport/series_stats.h"
#include "Headset/XR/headset_session.h"
#include "Headset/XR/screen_geometry.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <Windows.h>
#include <unknwn.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

struct Headset::XR::HeadsetSession::Impl
{
    /* What of an image a swap chain gives the eye: an image which has been taken is waited for, then copied to, then given back. */
    enum class ImageState
    {
        ACQUIRED, /* taken, and the compositor has not said that it is done with it yet */
        IDLE,
        READY     /* the compositor is done with it: it can be copied to              */
    };

    /* The swap chain of one eye. */
    struct EyeSwapchain
    {
        uint32_t                              acquired_index = 0;
        bool                                  has_image      = false; /* an image has been given back, which is the one that is shown */
        std::vector<XrSwapchainImageD3D12KHR> image_vec;
        ImageState                            state          = ImageState::IDLE;
        XrSwapchain                           swapchain      = XR_NULL_HANDLE;
    };

    /* What the frames of the compositor do, for the log, a second at a time. Of the thread that runs the frames, but for the number of
     * the images that were given back, which the lock guards.
     */
    struct Timing
    {
        Stereo::Transport::SeriesStats end_cost       = Stereo::Transport::SeriesStats(2.0); /* of xrEndFrame, ms                         */
        Stereo::Transport::SeriesStats frame_interval = Stereo::Transport::SeriesStats(0.0); /* between the frames the runtime asks for  */
        Stereo::Transport::SeriesStats image_wait     = Stereo::Transport::SeriesStats(0.0); /* for images that are not there yet, ms     */
        double                         last_report    = 0.0;
        uint64_t                       last_serial    = 0;   /* of the images in the last frame that had them */
        Stereo::Transport::SeriesStats lock_wait      = Stereo::Transport::SeriesStats(2.0); /* for the lock that the layers need, ms     */
        uint64_t                       n_empty        = 0;   /* frames with no layers                         */
        uint64_t                       n_new          = 0;   /* frames with images that were not shown       */
        uint64_t                       n_repeated     = 0;   /* frames with the images of the frame before   */
        uint64_t                       n_skipped      = 0;   /* frames the runtime said not to render        */
        float                          period_ms      = 0.0f;
        Stereo::Transport::SeriesStats wait_cost      = Stereo::Transport::SeriesStats(0.0); /* of xrWaitFrame, ms                        */
    };

    ~Impl();

    void add_log(const std::string& in_text);

    /* Every image of the swap chains of the eyes is made black, and is shown as it is until something is copied to the next: what the images
     * hold when a swap chain is made is whatever was in the memory, which flickers. False, and the reason, if it could not be done.
     */
    bool clear_swapchain_images(std::string* out_error_ptr);

    /* The action that puts the screen where the head is, with the buttons it is on. */
    bool create_actions(std::string* out_error_ptr);

    /* The device and the queue of the adapter that the headset is on. */
    bool create_device(std::string* out_error_ptr);

    /* The runtime, started with the extensions the session needs. */
    bool create_instance(std::string* out_error_ptr);
    bool create_session (std::string* out_error_ptr);

    /* The space of the room and the space of the head. */
    bool create_spaces     (std::string* out_error_ptr);
    void destroy_swapchains();

    /* Ends the frame with the layers (none is a frame with nothing in it). */
    void end_frame(const XrTime&                              in_time,
                   const uint32_t&                            in_n_layers,
                   const XrCompositionLayerBaseHeader* const* in_layer_ptr_array);

    /* The headset of the runtime, and what it says of itself. */
    bool find_system(std::string* out_error_ptr);

    /* The text for what OpenXR returned. */
    std::string get_result_text(const XrResult& in_result) const;

    /* The part of the view which the screen fills, and where the head is, are put in the pose and the sizes of the layers. */
    bool locate_head(const XrTime& in_time);

    /* What failed in a frame is said once for each result: a headset that is not ready fails each frame the same way. */
    void log_failure(const char*     in_what_ptr,
                     const XrResult& in_result);
    void log_screen (const float&    in_aspect_ratio);

    /* The layer of an eye: the screen, a cylinder if the runtime has it, a quad otherwise. */
    void make_layer(const uint32_t& in_n_eye,
                    const XrPosef&  in_pose,
                    const float&    in_aspect_ratio);

    /* The layers of the frame: the screen, for each eye. Fewer if there is nothing to show yet. */
    uint32_t make_layers             (const XrTime&         in_time);
    bool     make_swapchains         (const uint32_t&       in_width,
                                      const uint32_t&       in_height,
                                      std::string*          out_error_ptr);
    void     on_session_state_changed(const XrSessionState& in_state);

    /* The format of the swap chains: the one with the bytes of the images as they are. */
    bool pick_format(std::string* out_error_ptr);
    void poll_events();

    /* Reads the button that puts the screen where the head is. */
    bool read_recenter();

    /* The size of the image of an eye that the runtime recommends: what the headset is meant to be rendered at. False if it does not say. */
    bool read_recommended_eye_size(uint32_t* out_width_ptr,
                                   uint32_t* out_height_ptr) const;

    /* Makes everything of a session, in the order that each part needs the one before. */
    bool set_up(std::string* out_error_ptr);

    /* What the eyes see of the view is what the screen is to fill; read from the eyes as they are now, since a headset can change it. */
    void update_field_of_view(const XrTime& in_time);

    static const char* get_state_name(const XrSessionState& in_state);

    XrAction                                   action                      = XR_NULL_HANDLE;
    XrActionSet                                action_set                  = XR_NULL_HANDLE;
    XrCompositionLayerCylinderKHR              cylinder_layer_array[2]     = {};
    Microsoft::WRL::ComPtr<ID3D12Device>       device_ptr;
    std::array<EyeSwapchain, 2>                eye_array;
    HeadsetSession::FrameFunc                  frame_func;
    Headset::XR::FieldOfView                    field_of_view;
    int64_t                                    format                      = 0;
    bool                                       has_field_of_view           = false;
    bool                                       has_logged_screen           = false;
    XrInstance                                 instance                    = XR_NULL_HANDLE;
    bool                                       is_cylinder_supported       = false;
    std::atomic<bool>                          is_exit_requested           = {false};
    bool                                       is_placed                   = false;
    bool                                       is_running                  = false;
    XrSpace                                    local_space                 = XR_NULL_HANDLE;
    std::condition_variable                    image_condition;       /* told when the images of the eyes are given back */
    std::vector<std::string>                   log_line_vec;
    std::mutex                                 log_mutex;
    std::set<int>                              logged_result_set;
    std::atomic<float>                         ipd                         = {0.0f}; /* between the eyes of the person, in metres, smoothed; 0: not known */
    uint64_t                                   image_serial                = 0; /* how many times the images of the eyes were given back; the lock guards it */
    uint32_t                                   max_height                  = 0;
    uint32_t                                   max_width                   = 0;
    std::mutex                                 mutex;                 /* guards is_running, the swap chains and the layers */
    XrCompositionLayerQuad                     quad_layer_array[2]         = {};
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_ptr;
    std::string                                runtime_name;
    float                                      screen_position_fp32vec3[3] = {};
    float                                      screen_yaw                  = 0.0f;
    ::XrSession                                session               = XR_NULL_HANDLE;
    uint32_t                                   swapchain_height      = 0;
    uint32_t                                   swapchain_width       = 0;
    std::unique_ptr<Headset::D3D12::D3D12TextureClearer> texture_clearer_ptr; /* made with the first swap chain */
    Timing                                     timing;
    XrSystemId                                 system_id             = XR_NULL_SYSTEM_ID;
    std::string                                system_name;
    XrSpace                                    view_space            = XR_NULL_HANDLE;
};

#endif /* HEADSET_XR_HEADSET_SESSION_IMPL_H */

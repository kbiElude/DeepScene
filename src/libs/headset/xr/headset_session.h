/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef HEADSET_XR_HEADSET_SESSION_H
#define HEADSET_XR_HEADSET_SESSION_H

#include "utils/d3d12/d3d12_texture_clearer.h"
#include "utils/statistics/series_stats.h"
#include "headset/xr/screen_geometry.h"
#include "headset/xr/types.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>
#include <Windows.h>
#include <unknwn.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace Headset
{
    namespace XR
    {
        /* What a runtime says of the headset it has: enough to list it. */
        struct HeadsetInfo
        {
            uint32_t    eye_height = 0;  /* of the image of an eye that the runtime recommends, in pixels; 0 if it does not say */
            uint32_t    eye_width  = 0;
            std::string runtime_name;    /* of the OpenXR runtime */
            std::string system_name;     /* of the headset        */
        };

        /* A headset of an OpenXR runtime, and what is shown in it: a screen in the room, with the image of each eye on it, which stays where it
         * was put (it does not follow the head) and is as large as the view lets it be. Whichever headset the runtime has, as it is
         * asked for no more than OpenXR and its extension for Direct3D 12 give.
         *
         * Two threads use it: one runs the frames of the compositor (run_frame(), which waits for the compositor), and one gives it the
         * images of the eyes (acquire_eye_textures(), release_eye_textures()). The images of the eyes are textures of the device of the
         * session, which the images are copied to by whatever has the queue of the session.
         */
        class HeadsetSession
        {
        public:
            /* Makes the session, with its device, if there is a runtime and a headset it knows of. Returns nullptr, and says why, if there
             * is not.
             */
            static std::unique_ptr<HeadsetSession> create_ptr(std::string* out_error_ptr);

            /* Called from the thread that runs the frames, each time the compositor says that it wants one (the period is how long a frame
             * of the headset is, in milliseconds). It is to be set before the frames are run, and not to take long.
             */
            typedef std::function<void(const double& in_period_n_milliseconds)> FrameFunc;

            ~HeadsetSession();

            HeadsetSession(const HeadsetSession&)            = delete;
            HeadsetSession& operator=(const HeadsetSession&) = delete;

            /* Takes the textures of an eye image of the size to copy the eyes to: the next image of the swap chain of each. False if there
             * are none now (the session is not running, the compositor has not given one back yet) or the size is not one the headset can
             * show, and the reason. Every call that gives textures is followed by release_eye_textures(), which shows them.
             */
            bool acquire_eye_textures(const uint32_t&  in_width,
                                      const uint32_t&  in_height,
                                      ID3D12Resource** out_left_texture_ptr_ptr,
                                      ID3D12Resource** out_right_texture_ptr_ptr,
                                      std::string*     out_error_ptr);

            /* What the runtime has the device and the queue be: the device is of the adapter that the headset is on. */
            ID3D12Device*       get_device_ptr() const;
            ID3D12CommandQueue* get_queue_ptr () const;

            /* What the runtime calls the headset. */
            std::string get_system_name() const;

            /* The distance between the eyes of the person who wears the headset, in metres, as the runtime says it (smoothed, and it follows an
             * adjustment of the headset while it is worn); 0 until it has said it.
             */
            float get_ipd() const;

            /* Returns the textures given by acquire_eye_textures(), with what was copied to them in them, to be shown. */
            void release_eye_textures();

            /* Ends the session: the next run_frame() returns false. Can be called from any thread. */
            void request_exit();

            /* A frame of the compositor: waits for it, and gives it the screen. Returns false when the session has ended, because the user
             * ended it, the runtime did, or request_exit() was called; the reason, if there is one, is the error. */
            bool run_frame(std::string* out_error_ptr);

            void set_frame_func(const FrameFunc& in_frame_func);

            /* What the session has to tell since it was last asked: the states it went through, the size and the place of the screen, what
             * went wrong. Any thread.
             */
            std::vector<std::string> take_log_lines();


            /* The state that the textures are in when they are given, and have to be in when they are released. */
            static D3D12_RESOURCE_STATES get_texture_state();

            /* Asks the runtime whether it has a headset, and what it is, without a session: a few moments. Returns false, and says why, if
             * there is no runtime, or it has no headset connected.
             */
            static bool probe(HeadsetInfo* out_info_ptr,
                              std::string* out_error_ptr);

        private:
            /* The swap chain of one eye. */
            struct EyeSwapchain
            {
                uint32_t                              acquired_index = 0;
                bool                                  has_image      = false; /* an image has been given back, which is the one that is shown */
                std::vector<XrSwapchainImageD3D12KHR> image_vec;
                SwapchainImageState                   state          = SwapchainImageState::IDLE;
                XrSwapchain                           swapchain      = XR_NULL_HANDLE;
            };

            /* The log lines, until they are asked for. */
            struct Log
            {
                std::vector<std::string> line_vec;
                std::mutex               mutex;
            };

            /* What the thread that runs the frames and the one that gives the images of the eyes both use. The mutex guards all of it. */
            struct Shared
            {
                XrCompositionLayerCylinderKHR cylinder_layer_array[2] = {};
                std::array<EyeSwapchain, 2>   eye_array;
                std::condition_variable       image_condition;                      /* told when the images of the eyes are given back                      */
                uint64_t                      image_serial            = 0;          /* how many times the images of the eyes were given back                */
                bool                          is_running              = false;
                std::mutex                    mutex;
                XrCompositionLayerQuad        quad_layer_array[2]     = {};
            };

            /* What the frames of the compositor do, for the log, a second at a time. Of the thread that runs the frames, but for the number of
             * the images that were given back, which the lock guards.
             */
            struct Timing
            {
                Utils::Statistics::SeriesStats end_cost       = Utils::Statistics::SeriesStats(2.0); /* of xrEndFrame, ms                       */
                Utils::Statistics::SeriesStats frame_interval = Utils::Statistics::SeriesStats(0.0); /* between the frames the runtime asks for */
                Utils::Statistics::SeriesStats image_wait     = Utils::Statistics::SeriesStats(0.0); /* for images that are not there yet, ms   */
                double                         last_report    = 0.0;
                uint64_t                       last_serial    = 0;                                   /* of the images in the last frame that had them */
                Utils::Statistics::SeriesStats lock_wait      = Utils::Statistics::SeriesStats(2.0); /* for the lock that the layers need, ms         */
                uint64_t                       n_empty        = 0;                                   /* frames with no layers                         */
                uint64_t                       n_new          = 0;                                   /* frames with images that were not shown        */
                uint64_t                       n_repeated     = 0;                                   /* frames with the images of the frame before    */
                uint64_t                       n_skipped      = 0;                                   /* frames the runtime said not to render         */
                float                          period_ms      = 0.0f;
                Utils::Statistics::SeriesStats wait_cost      = Utils::Statistics::SeriesStats(0.0); /* of xrWaitFrame, ms */
            };

            HeadsetSession();

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


            /* m_ipd and m_is_exit_requested are atomics that any thread reads and writes; m_shared and m_log have their own locks; the rest
             * is of the thread that sets the session up and then of the one that runs the frames.
             */
            XrAction                                           m_action                      = XR_NULL_HANDLE;
            XrActionSet                                        m_action_set                  = XR_NULL_HANDLE;
            Microsoft::WRL::ComPtr<ID3D12Device>               m_device_ptr;
            Headset::XR::FieldOfView                           m_field_of_view;
            int64_t                                            m_format                      = 0;
            FrameFunc                                          m_frame_func;
            bool                                               m_has_field_of_view           = false;
            bool                                               m_has_logged_screen           = false;
            XrInstance                                         m_instance                    = XR_NULL_HANDLE;
            std::atomic<float>                                 m_ipd                         = {0.0f}; /* between the eyes of the person, in metres, smoothed; 0: not known */
            bool                                               m_is_cylinder_supported       = false;
            std::atomic<bool>                                  m_is_exit_requested           = {false};
            bool                                               m_is_placed                   = false;
            XrSpace                                            m_local_space                 = XR_NULL_HANDLE;
            Log                                                m_log;
            std::set<int>                                      m_logged_result_set;
            uint32_t                                           m_max_height                  = 0;
            uint32_t                                           m_max_width                   = 0;
            Microsoft::WRL::ComPtr<ID3D12CommandQueue>         m_queue_ptr;
            std::string                                        m_runtime_name;
            float                                              m_screen_position_fp32vec3[3] = {};
            float                                              m_screen_yaw                  = 0.0f;
            ::XrSession                                        m_session                     = XR_NULL_HANDLE;
            Shared                                             m_shared;
            uint32_t                                           m_swapchain_height            = 0;
            uint32_t                                           m_swapchain_width             = 0;
            XrSystemId                                         m_system_id                   = XR_NULL_SYSTEM_ID;
            std::string                                        m_system_name;
            std::unique_ptr<Utils::D3D12::D3D12TextureClearer> m_texture_clearer_ptr;                 /* made with the first swap chain */
            Timing                                             m_timing;
            XrSpace                                            m_view_space                  = XR_NULL_HANDLE;
        };
    }
}

#endif /* HEADSET_XR_HEADSET_SESSION_H */

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
        /* What a runtime reports about its headset: enough to list it. */
        struct HeadsetInfo
        {
            uint32_t    eye_height = 0;  /* the recommended image height per eye, in pixels; 0 if the runtime does not say */
            uint32_t    eye_width  = 0;
            std::string runtime_name;    /* name of the OpenXR runtime */
            std::string system_name;     /* name of the headset        */
        };

        /* OpenXR headset session showing a fixed screen in the room with the image of each eye (OpenXR + Direct3D 12 only).
         * Two threads: one calls run_frame(), the other acquire_eye_textures()/release_eye_textures().
         * The queue's owner copies the eye images into the textures of the session's device.
         */
        class HeadsetSession
        {
        public:
            /* Returns nullptr and sets out_error_ptr if no runtime or headset is available. */
            static std::unique_ptr<HeadsetSession> create_ptr(std::string* out_error_ptr);

            /* Called on the frame thread for each compositor frame (argument: headset frame period in ms). Set before running frames; must return quickly. */
            typedef std::function<void(const double& in_period_n_milliseconds)> FrameFunc;

            ~HeadsetSession();

            HeadsetSession(const HeadsetSession&)            = delete;
            HeadsetSession& operator=(const HeadsetSession&) = delete;

            /* Acquires the next swap chain image of each eye for the given size. Returns false and sets out_error_ptr if none is available or
             * the size is unsupported. Every successful call must be followed by release_eye_textures().
             */
            bool acquire_eye_textures(const uint32_t&  in_width,
                                      const uint32_t&  in_height,
                                      ID3D12Resource** out_left_texture_ptr_ptr,
                                      ID3D12Resource** out_right_texture_ptr_ptr,
                                      std::string*     out_error_ptr);

            /* Device and queue of the session, on the headset's adapter. */
            ID3D12Device*       get_device_ptr() const;
            ID3D12CommandQueue* get_queue_ptr () const;

            std::string get_system_name() const;

            /* Smoothed distance between the eyes in metres; 0 until the runtime has reported it. */
            float get_ipd() const;

            /* Shows the textures that acquire_eye_textures() returned. */
            void release_eye_textures();

            /* Makes the next run_frame() return false. Any thread. */
            void request_exit();

            /* Waits for and runs one compositor frame. Returns false when the session has ended (out_error_ptr is not set). */
            bool run_frame(std::string* out_error_ptr);

            void set_frame_func(const FrameFunc& in_frame_func);

            /* Returns and clears the log lines collected so far. Any thread. */
            std::vector<std::string> take_log_lines();


            /* State of the acquired textures; they must be back in it when released. */
            static D3D12_RESOURCE_STATES get_texture_state();

            /* Queries the runtime for a headset without creating a session (slow). Returns false and sets out_error_ptr if there is none. */
            static bool probe(HeadsetInfo* out_info_ptr,
                              std::string* out_error_ptr);

        private:
            /* The swap chain of one eye. */
            struct EyeSwapchain
            {
                uint32_t                              acquired_index = 0;
                bool                                  has_image      = false; /* an image was released and is the one shown */
                std::vector<XrSwapchainImageD3D12KHR> image_vec;
                SwapchainImageState                   state          = SwapchainImageState::IDLE;
                XrSwapchain                           swapchain      = XR_NULL_HANDLE;
            };

            /* The log lines, kept until they are taken. */
            struct Log
            {
                std::vector<std::string> line_vec;
                std::mutex               mutex;
            };

            /* State of both threads; the mutex guards all of it. */
            struct Shared
            {
                XrCompositionLayerCylinderKHR cylinder_layer_array[2] = {};
                std::array<EyeSwapchain, 2>   eye_array;
                std::condition_variable       image_condition;                      /* notified when the eye images are released                            */
                uint64_t                      image_serial            = 0;          /* how many times the eye images were released                          */
                bool                          is_running              = false;
                std::mutex                    mutex;
                XrCompositionLayerQuad        quad_layer_array[2]     = {};
            };

            /* Frame statistics, logged once per second. Frame thread only (reads Shared::image_serial under the lock). */
            struct Timing
            {
                Utils::Statistics::SeriesStats end_cost       = Utils::Statistics::SeriesStats(2.0); /* of xrEndFrame, ms                       */
                Utils::Statistics::SeriesStats frame_interval = Utils::Statistics::SeriesStats(0.0); /* between the frames the runtime asks for */
                Utils::Statistics::SeriesStats image_wait     = Utils::Statistics::SeriesStats(0.0); /* waiting for new images, ms              */
                double                         last_report    = 0.0;
                uint64_t                       last_serial    = 0;                                   /* image serial of the last frame that had images */
                Utils::Statistics::SeriesStats lock_wait      = Utils::Statistics::SeriesStats(2.0); /* waiting for the lock that the layers need, ms */
                uint64_t                       n_empty        = 0;                                   /* frames with no layers                         */
                uint64_t                       n_new          = 0;                                   /* frames with images that were not shown before */
                uint64_t                       n_repeated     = 0;                                   /* frames with the images of the frame before    */
                uint64_t                       n_skipped      = 0;                                   /* frames the runtime said not to render         */
                float                          period_ms      = 0.0f;
                Utils::Statistics::SeriesStats wait_cost      = Utils::Statistics::SeriesStats(0.0); /* of xrWaitFrame, ms */
            };

            HeadsetSession();

            void add_log(const std::string& in_text);

            /* Clears all swap chain images to black (new images hold garbage that would flicker). */
            bool clear_swapchain_images(std::string* out_error_ptr);

            /* Creates the recenter action and suggests its buttons. */
            bool create_actions(std::string* out_error_ptr);

            /* Creates the device and queue on the headset's adapter. */
            bool create_device(std::string* out_error_ptr);

            /* Starts the runtime with the needed extensions. */
            bool create_instance(std::string* out_error_ptr);
            bool create_session (std::string* out_error_ptr);

            /* Creates the room (local) and head (view) spaces. */
            bool create_spaces     (std::string* out_error_ptr);
            void destroy_swapchains();

            /* No layers: the frame shows nothing. */
            void end_frame(const XrTime&                              in_time,
                           const uint32_t&                            in_n_layers,
                           const XrCompositionLayerBaseHeader* const* in_layer_ptr_array);

            /* Finds the headset; reads its name and largest image size. */
            bool find_system(std::string* out_error_ptr);

            /* Returns the text for an OpenXR result code. */
            std::string get_result_text(const XrResult& in_result) const;

            /* Reads head position and yaw (screen placement). Returns false if the pose is invalid. */
            bool locate_head(const XrTime& in_time);

            /* Logs each distinct result code once (a not-ready headset fails every frame). */
            void log_failure(const char*     in_what_ptr,
                             const XrResult& in_result);
            void log_screen (const float&    in_aspect_ratio);

            /* Cylinder layer if supported, quad otherwise. */
            void make_layer(const uint32_t& in_n_eye,
                            const XrPosef&  in_pose,
                            const float&    in_aspect_ratio);

            /* Returns the number of layers; 0 if there is nothing to show. */
            uint32_t make_layers             (const XrTime&         in_time);
            bool     make_swapchains         (const uint32_t&       in_width,
                                              const uint32_t&       in_height,
                                              std::string*          out_error_ptr);
            void     on_session_state_changed(const XrSessionState& in_state);

            /* Picks the format whose bytes match the images. */
            bool pick_format(std::string* out_error_ptr);
            void poll_events();

            /* True if the recenter button was just pressed. */
            bool read_recenter();

            /* Recommended image size per eye; false if the runtime does not say. */
            bool read_recommended_eye_size(uint32_t* out_width_ptr,
                                           uint32_t* out_height_ptr) const;

            /* Runs the creation steps in dependency order. */
            bool set_up(std::string* out_error_ptr);

            /* Reads the shared field of view (every frame, it can change) and updates the smoothed eye distance. */
            void update_field_of_view(const XrTime& in_time);

            static const char* get_state_name(const XrSessionState& in_state);


            /* m_ipd and m_is_exit_requested are atomic; m_shared and m_log have their own locks. The rest: setup thread, then frame thread. */
            XrAction                                           m_action                      = XR_NULL_HANDLE;
            XrActionSet                                        m_action_set                  = XR_NULL_HANDLE;
            Microsoft::WRL::ComPtr<ID3D12Device>               m_device_ptr;
            Headset::XR::FieldOfView                           m_field_of_view;
            int64_t                                            m_format                      = 0;
            FrameFunc                                          m_frame_func;
            bool                                               m_has_field_of_view           = false;
            bool                                               m_has_logged_screen           = false;
            XrInstance                                         m_instance                    = XR_NULL_HANDLE;
            std::atomic<float>                                 m_ipd                         = {0.0f}; /* eye distance of the person, in metres, smoothed; 0: not known */
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
            std::unique_ptr<Utils::D3D12::D3D12TextureClearer> m_texture_clearer_ptr;                 /* created on first use */
            Timing                                             m_timing;
            XrSpace                                            m_view_space                  = XR_NULL_HANDLE;
        };
    }
}

#endif /* HEADSET_XR_HEADSET_SESSION_H */

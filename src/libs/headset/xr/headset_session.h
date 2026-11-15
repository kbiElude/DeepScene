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

        /* A headset of an OpenXR runtime and what is shown in it: a screen in the room that carries the image of each eye. The screen stays
         * where it was placed (it does not follow the head) and is as large as the view allows. It works with any headset, because it
         * relies only on OpenXR and its Direct3D 12 extension.
         *
         * Two threads use it. One runs the frames of the compositor (run_frame(), which waits for the compositor). The other supplies the
         * eye images (acquire_eye_textures(), release_eye_textures()). The eye images are textures of the session's device. Whatever owns
         * the session's queue copies the images into them.
         */
        class HeadsetSession
        {
        public:
            /* Creates the session, with its device, if a runtime and a headset are available. Returns nullptr and sets out_error_ptr to the
             * reason if they are not.
             */
            static std::unique_ptr<HeadsetSession> create_ptr(std::string* out_error_ptr);

            /* Called from the thread that runs the frames, each time the compositor wants a frame. The argument is the duration of a headset
             * frame in milliseconds. Set it before the frames are run. It must return quickly.
             */
            typedef std::function<void(const double& in_period_n_milliseconds)> FrameFunc;

            ~HeadsetSession();

            HeadsetSession(const HeadsetSession&)            = delete;
            HeadsetSession& operator=(const HeadsetSession&) = delete;

            /* Acquires the textures to copy the eyes into: the next image of each eye's swap chain, for an eye image of the given size.
             * Returns false and sets out_error_ptr to the reason if no textures are available (the session is not running, or the compositor
             * has not given an image back) or if the headset cannot show the size. Every call that returns textures must be followed by
             * release_eye_textures(), which shows them.
             */
            bool acquire_eye_textures(const uint32_t&  in_width,
                                      const uint32_t&  in_height,
                                      ID3D12Resource** out_left_texture_ptr_ptr,
                                      ID3D12Resource** out_right_texture_ptr_ptr,
                                      std::string*     out_error_ptr);

            /* The device and the queue that the session uses. The device is on the adapter that the headset is on. */
            ID3D12Device*       get_device_ptr() const;
            ID3D12CommandQueue* get_queue_ptr () const;

            /* The name that the runtime gives the headset. */
            std::string get_system_name() const;

            /* The distance between the eyes of the person wearing the headset, in metres, as the runtime reports it. It is smoothed and
             * follows adjustments of the headset while it is worn. It is 0 until the runtime has reported it.
             */
            float get_ipd() const;

            /* Releases the textures that acquire_eye_textures() returned, so that the compositor shows what was copied into them. */
            void release_eye_textures();

            /* Ends the session: the next run_frame() returns false. Can be called from any thread. */
            void request_exit();

            /* Runs one frame of the compositor: waits for it and gives it the screen. Returns false when the session has ended (the user
             * ended it, the runtime ended it, or request_exit() was called). out_error_ptr is not set.
             */
            bool run_frame(std::string* out_error_ptr);

            void set_frame_func(const FrameFunc& in_frame_func);

            /* Returns the log lines collected since the last call: the states the session went through, the size and place of the screen,
             * and what went wrong. Callable from any thread.
             */
            std::vector<std::string> take_log_lines();


            /* The resource state that the textures are in when acquire_eye_textures() hands them out. They must be in it again when they are
             * released.
             */
            static D3D12_RESOURCE_STATES get_texture_state();

            /* Asks the runtime whether it has a headset, and which one, without creating a session. It takes a few moments. Returns false and
             * sets out_error_ptr to the reason if there is no runtime or no headset is connected.
             */
            static bool probe(HeadsetInfo* out_info_ptr,
                              std::string* out_error_ptr);

        private:
            /* The swap chain of one eye. */
            struct EyeSwapchain
            {
                uint32_t                              acquired_index = 0;
                bool                                  has_image      = false; /* an image was released to the compositor; it is the one that is shown */
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

            /* The state that both threads use: the thread that runs the frames and the thread that supplies the eye images. The mutex guards
             * all of it.
             */
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

            /* Statistics about the frames of the compositor, reported in the log once per second. Only the thread that runs the frames uses
             * it. It reads the image serial of Shared, which the lock guards.
             */
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

            /* Clears every image of the eyes' swap chains to black, so that black is shown until something is copied into the next image. The
             * images of a new swap chain hold whatever was in memory, which would flicker. Returns false and sets out_error_ptr to the reason
             * if it fails.
             */
            bool clear_swapchain_images(std::string* out_error_ptr);

            /* Creates the action that puts the screen where the head is, and suggests the buttons that trigger it. */
            bool create_actions(std::string* out_error_ptr);

            /* Creates the device and the command queue on the adapter that the headset is on. */
            bool create_device(std::string* out_error_ptr);

            /* Starts the OpenXR runtime with the extensions that the session needs. */
            bool create_instance(std::string* out_error_ptr);
            bool create_session (std::string* out_error_ptr);

            /* Creates the reference spaces: one for the room (local) and one for the head (view). */
            bool create_spaces     (std::string* out_error_ptr);
            void destroy_swapchains();

            /* Ends the frame with the given layers. With no layers the frame shows nothing. */
            void end_frame(const XrTime&                              in_time,
                           const uint32_t&                            in_n_layers,
                           const XrCompositionLayerBaseHeader* const* in_layer_ptr_array);

            /* Finds the headset of the runtime and reads its properties (name and largest image size). */
            bool find_system(std::string* out_error_ptr);

            /* Returns the text for an OpenXR result code. */
            std::string get_result_text(const XrResult& in_result) const;

            /* Reads where the head is (position and yaw), which is where the screen is placed. Returns false if the pose is not valid. */
            bool locate_head(const XrTime& in_time);

            /* Logs a failed call once for each distinct result code. A headset that is not ready fails the same way on every frame. */
            void log_failure(const char*     in_what_ptr,
                             const XrResult& in_result);
            void log_screen (const float&    in_aspect_ratio);

            /* Builds the layer of one eye: the screen as a cylinder if the runtime supports it, as a quad otherwise. */
            void make_layer(const uint32_t& in_n_eye,
                            const XrPosef&  in_pose,
                            const float&    in_aspect_ratio);

            /* Builds the layers of the frame: the screen for each eye. Returns the number of layers, which is 0 if there is nothing to show. */
            uint32_t make_layers             (const XrTime&         in_time);
            bool     make_swapchains         (const uint32_t&       in_width,
                                              const uint32_t&       in_height,
                                              std::string*          out_error_ptr);
            void     on_session_state_changed(const XrSessionState& in_state);

            /* Picks the swap chain format whose bytes match the images. */
            bool pick_format(std::string* out_error_ptr);
            void poll_events();

            /* Returns true if the user just pressed the button that puts the screen where the head is. */
            bool read_recenter();

            /* Returns the image size per eye that the runtime recommends, which is the size the headset is meant to be rendered at. Returns
             * false if the runtime does not say.
             */
            bool read_recommended_eye_size(uint32_t* out_width_ptr,
                                           uint32_t* out_height_ptr) const;

            /* Creates everything the session needs, in order. Each step depends on the one before it. */
            bool set_up(std::string* out_error_ptr);

            /* Reads the field of view that both eyes share, which is what the screen fills. It is read on every frame because a headset can
             * change it. Also updates the smoothed eye distance.
             */
            void update_field_of_view(const XrTime& in_time);

            static const char* get_state_name(const XrSessionState& in_state);


            /* m_ipd and m_is_exit_requested are atomics that any thread can read and write. m_shared and m_log have their own locks. The rest
             * belongs first to the thread that sets the session up and then to the thread that runs the frames.
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

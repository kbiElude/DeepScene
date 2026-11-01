/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOXR_HEADSET_SESSION_H
#define STEREOXR_HEADSET_SESSION_H

#include <functional>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>
#include <d3d12.h>

namespace Stereo
{
    namespace StereoXR
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

            void set_frame_func(const FrameFunc& in_frame_func);

            /* A frame of the compositor: waits for it, and gives it the screen. Returns false when the session has ended, because the user
             * ended it, the runtime did, or request_exit() was called; the reason, if there is one, is the error. */
            bool run_frame(std::string* out_error_ptr);

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
            struct Impl;

            explicit HeadsetSession(std::unique_ptr<Impl> in_impl_ptr);


            std::unique_ptr<Impl> m_impl_ptr;
        };
    }
}

#endif /* STEREOXR_HEADSET_SESSION_H */

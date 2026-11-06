/* API Interceptor (c) 2025 Dominik Witczak
/ *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef VRPLAYER_APP_SESSION_H
#define VRPLAYER_APP_SESSION_H

#include "VR/AppCatalog/types.h"
#include "Stereo/Transport/frame_channel.h"
#include "types.h"
#include <memory>
#include <string>
#include <Windows.h>

namespace VR
{
    namespace VRPlayer
    {
        /* One application played in VR: the window which shows its images is started, and the launcher, which starts the application
         * with the converter in it; they are watched until the application exits or the window is closed. The launcher of the
         * architecture of the application is the one which starts it (see StereoLauncher). Everything belongs to a job, so that
         * ending the session, or this program, ends all of it.
         */
        class AppSession
        {
        public:
            AppSession();
            ~AppSession();

            /* What went wrong, if the state is FAILED. */
            const std::string& get_message() const;

            SessionState get_state() const;

            /* How strong the 3D is: 1 is the natural one the converter works out for each scene (see Stereo::Core::set_depth_strength), 0 is
             * none. It can be said while the application plays; it is ignored when none does.
             */
            void set_depth_strength(const float& in_depth_strength);

            /* Whether the draws that the recipe leaves alone (an interface, text) are pushed toward the viewer, by what the recipe says. It can
             * be said while the application plays; it is ignored when none does.
             */
            void set_is_overlay_pushed(const bool& in_is_pushed);

            /* Starts the window and the launcher. With the headset the window shows the images in the headset of the OpenXR runtime too, if
             * there is one. The render scale is how many times larger than the application asks the converter renders,
             * in each direction (see ResolutionPolicy); it is said to the converter before the application starts. Returns false, and says
             * why, if that cannot be done.
             */
            bool start(const VR::AppCatalog::AppEntry& in_app,
                       const std::string&              in_launcher_directory,
                       const uint32_t&                 in_target_eye_width,
                       const uint32_t&                 in_target_eye_height,
                       const bool&                     in_use_headset,
                       const bool&                     in_pace_to_headset,
                       const bool&                     in_write_log,
                       std::string*                    out_error_ptr);

            /* Looks whether the launcher has returned or the window is closed; call it often. */
            void update();

            /* The command lines of the launcher and of the window. */
            static std::string get_command_line       (const SessionRequest& in_request);
            static std::string get_presenter_command_line(const SessionRequest& in_request);

        private:
            void close_handles();
            void end_session  (const SessionState& in_state,
                               const std::string&  in_message);

            static std::string quote_argument(const std::string& in_argument);
            static std::string read_output   (const HANDLE&      in_pipe);

            std::unique_ptr<Stereo::Transport::FrameChannel> m_channel_ptr; /* what the converter and the window share */
            float                                            m_depth_strength; /* the last one said, if m_has_depth_strength */
            bool                                             m_has_depth_strength;
            bool                                             m_has_is_overlay_pushed;
            bool                                             m_is_overlay_pushed; /* the last one said, if m_has_is_overlay_pushed */
            HANDLE                                           m_job;
            HANDLE                                           m_launcher_process;
            std::string                                      m_message;
            HANDLE                                           m_output_pipe;
            SessionState                                     m_state;
            HANDLE                                           m_presenter_process;
        };
    }
}

#endif /* VRPLAYER_APP_SESSION_H */

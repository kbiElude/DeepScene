/* API Interceptor (c) 2025 Dominik Witczak
/ *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef VRPLAYER_APP_SESSION_H
#define VRPLAYER_APP_SESSION_H

#include "vr_player/app_catalog/types.h"
#include "stereo/transport/frame_channel.h"
#include "types.h"
#include <memory>
#include <string>
#include <Windows.h>

namespace VRPlayer
{
    /* One application played in VR. The session starts the window that shows the images of the application, and the launcher, which starts the
     * application with the converter inside it. Both are watched until the application exits or the window is closed. The launcher that
     * matches the architecture of the application starts it (see StereoLauncher). Everything belongs to one job, so ending the session,
     * or this program, ends all of it.
     */
    class AppSession
    {
    public:
         AppSession();
        ~AppSession();

        /* The reason for the failure, if the state is FAILED. */
        const std::string& get_message() const;

        SessionState get_state() const;

        /* Sets how strong the 3D is: 1 is the natural depth the converter works out for each scene (see Stereo::Core::set_depth_strength), 0 is
         * no depth. It can be set while the application plays. It is ignored when no session is running.
         */
        void set_depth_strength(const float& in_depth_strength);

        /* Sets whether the draws that the recipe leaves alone (an interface, text) are pushed toward the viewer by the amount the recipe gives.
         * It can be set while the application plays. It is ignored when no session is running.
         */
        void set_is_overlay_pushed(const bool& in_is_pushed);

        /* Starts the window and the launcher. With in_use_headset the window also shows the images in the headset of the OpenXR runtime, if
         * there is one. The target eye size is the size, in pixels, at which the converter should render each eye (see ResolutionPolicy). It is
         * sent to the converter before the application starts, and it is ignored if either value is 0. Returns false and sets the error if the
         * session cannot be started.
         */
        bool start(const VRPlayer::AppCatalog::AppEntry& in_app,
                   const std::string&                    in_launcher_directory,
                   const uint32_t&                       in_target_eye_width,
                   const uint32_t&                       in_target_eye_height,
                   const bool&                           in_use_headset,
                   const bool&                           in_pace_to_headset,
                   const bool&                           in_write_log,
                   std::string*                          out_error_ptr);

        /* Checks whether the launcher has exited or the window was closed. Call it often. */
        void update();

        /* Build the command lines of the launcher and of the window. */
        static std::string get_command_line          (const SessionRequest& in_request);
        static std::string get_presenter_command_line(const SessionRequest& in_request);

    private:
        void close_handles();
        void end_session  (const SessionState& in_state,
                           const std::string&  in_message);

        static std::string quote_argument(const std::string& in_argument);
        static std::string read_output   (const HANDLE&      in_pipe);

        std::unique_ptr<Stereo::Transport::FrameChannel> m_channel_ptr; /* shared by the converter and the window */
        float                                            m_depth_strength; /* the last value sent, valid if m_has_depth_strength */
        bool                                             m_has_depth_strength;
        bool                                             m_has_is_overlay_pushed;
        bool                                             m_is_overlay_pushed; /* the last value sent, valid if m_has_is_overlay_pushed */
        HANDLE                                           m_job;
        HANDLE                                           m_launcher_process;
        std::string                                      m_message;
        HANDLE                                           m_output_pipe;
        SessionState                                     m_state;
        HANDLE                                           m_presenter_process;
    };
}

#endif /* VRPLAYER_APP_SESSION_H */

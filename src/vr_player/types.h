/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef VRPLAYER_TYPES_H
#define VRPLAYER_TYPES_H

#include "vr_player/app_catalog/types.h"
#include "headset/vr_devices/types.h"
#include <string>
#include <vector>

namespace VRPlayer
{
    struct Options
    {
        std::string catalog_filename; /* empty: the catalog next to the program is used */
        bool        is_logging = false; /* the converter and the presenter write a log of each session (--log) */
    };

    /* What is needed to start an application with the stereo converter in it. */
    struct SessionRequest
    {
        std::string arguments;          /* command line arguments of the application                            */
        std::string executable_path;
        std::string launch_button_text; /* text of a button in the first dialog of the application, which the launcher presses; empty: none */
        std::string launcher_path;      /* path of the StereoLauncher that matches the application architecture */
        bool        is_logging          = false; /* the converter and the presenter write a log of the session  */
        bool        pace_to_headset     = true;  /* the application renders one frame per headset frame         */
        std::string recipe_path;
        std::string session_id;
        std::string title;              /* title of the window that shows the images                             */
        std::string untick_checkbox_text; /* text of a checkbox in the same dialog, which the launcher unticks; empty: none */
        bool        use_headset         = false; /* the window also shows them in the headset, if there is one   */
        std::string presenter_path;        /* path of StereoPresenter, which shows the images of the application */
        std::string working_directory;
    };

    enum class SessionState
    {
        ENDED,   /* the application was started, and has exited */
        FAILED,  /* it could not be started                     */
        IDLE,    /* nothing was started                         */
        RUNNING
    };

    /* The data that the window shows. */
    struct WindowContent
    {
        VRPlayer::AppCatalog::Catalog             catalog;
        std::string                               catalog_error;        /* empty if the catalog was read             */
        std::string                               device_error;         /* empty if the devices were listed          */
        std::vector<Headset::VRDevices::VrDevice> device_vec;
        bool                                      is_logging = false;   /* a log is written for each session         */
        std::string                               launcher_directory;   /* where the launchers and the presenter are */
    };
}

#endif /* VRPLAYER_TYPES_H */

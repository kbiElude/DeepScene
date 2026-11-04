/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef VRPLAYER_TYPES_H
#define VRPLAYER_TYPES_H

#include "VR/AppCatalog/types.h"
#include "VR/VRDevices/types.h"
#include <string>
#include <vector>

namespace VR
{
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
            std::string arguments;          /* of the application                                                          */
            std::string executable_path;
            std::string launcher_path;      /* StereoLauncher of the architecture of the application                       */
            bool        is_logging          = false; /* the converter and the presenter write a log of the session                    */
            bool        pace_to_headset     = true;  /* the application makes a frame for each one of the headset                    */
            std::string recipe_path;
            std::string session_id;
            std::string title;              /* of the window which shows the images                                        */
            bool        use_headset         = false; /* the window shows them in the headset too, if there is one         */
            std::string presenter_path;        /* StereoPresenter, which shows the images of the application                     */
            std::string working_directory;
        };

        enum class SessionState
        {
            ENDED,   /* the application was started, and has exited */
            FAILED,  /* it could not be started                     */
            IDLE,    /* nothing was started                         */
            RUNNING
        };

        /* What the window shows. */
        struct WindowContent
        {
            VR::AppCatalog::Catalog              catalog;
            std::string                          catalog_error;    /* empty if the catalog was read                    */
            std::string                          device_error;     /* empty if the devices were listed                 */
            std::vector<VR::VRDevices::VrDevice> device_vec;
            bool                                 is_logging = false;   /* a log is written for each session                  */
            std::string                          launcher_directory; /* where the launchers and the presenter are         */
        };
    }
}

#endif /* VRPLAYER_TYPES_H */

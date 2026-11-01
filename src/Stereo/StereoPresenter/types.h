/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOPRESENTER_TYPES_H
#define STEREOPRESENTER_TYPES_H

#include <string>

namespace Stereo
{
    namespace StereoPresenter
    {
        struct Options
        {
            std::string session_id;
            bool        use_log     = false; /* the presenter writes what it does to the log of the session; otherwise nothing is written */
            std::string title;       /* of the window; a default if empty */
            bool        use_headset = false; /* show the images in the headset of an OpenXR runtime too, if there is one */
            bool        use_pacing  = true;  /* with the headset: the application makes a frame for each one of the headset            */
        };
    }
}

#endif /* STEREOPRESENTER_TYPES_H */

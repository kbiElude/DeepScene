/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_PRESENTER_TYPES_H
#define STEREO_PRESENTER_TYPES_H

#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace Presenter
    {
        struct Options
        {
            uint32_t    eye_height  = 0;     /* the size per eye that the application is asked to render at (see FrameChannel); 0: none */
            uint32_t    eye_width   = 0;
            std::string session_id;
            bool        use_log     = false; /* the presenter writes what it does to the log of the session; otherwise nothing is written */
            std::string title;       /* of the window; a default if empty */
            bool        use_headset = false; /* show the images in the headset of an OpenXR runtime too, if there is one */
            bool        use_pacing  = true;  /* with the headset: the application renders one frame per headset frame                  */
        };
    }
}

#endif /* STEREO_PRESENTER_TYPES_H */

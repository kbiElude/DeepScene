/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APPTHUMBNAILS_TYPES_H
#define APPTHUMBNAILS_TYPES_H

#include <string>

namespace VRPlayer
{
    namespace AppThumbnails
    {
        /* A picture to have: where to get it, and where it is kept. */
        struct ThumbnailJob
        {
            std::string file_path;
            std::string url;
        };
    }
}

#endif /* APPTHUMBNAILS_TYPES_H */

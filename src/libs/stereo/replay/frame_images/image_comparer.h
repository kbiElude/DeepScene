/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Says how two images differ, and shows it.
 */
#ifndef STEREO_REPLAY_FRAMEIMAGES_IMAGE_COMPARER_H
#define STEREO_REPLAY_FRAMEIMAGES_IMAGE_COMPARER_H

#include "stereo/replay/frame_images/types.h"

namespace Stereo
{
    namespace Replay
    {
        namespace FrameImages
        {
            class ImageComparer
            {
            public:
                /* Images of other sizes differ in everything: has_same_size is false and the other fields are 0. */
                static ImageDifference compare(const Image& in_first_image,
                                               const Image& in_second_image);

                /* An image of the size of the two, black where they are the same and red where they differ. False, and an empty image, if their sizes
                 * differ.
                 */
                static bool make_difference_image(const Image& in_first_image,
                                                  const Image& in_second_image,
                                                  Image*       out_image_ptr);
            };
        }
    }
}

#endif /* STEREO_REPLAY_FRAMEIMAGES_IMAGE_COMPARER_H */

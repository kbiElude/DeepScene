/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Puts images together: the two eyes of a frame as an anaglyph, and many images in a grid.
 */
#ifndef STEREO_REPLAY_FRAMEIMAGES_IMAGE_COMPOSER_H
#define STEREO_REPLAY_FRAMEIMAGES_IMAGE_COMPOSER_H

#include "Stereo/Replay/FrameImages/types.h"
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace FrameImages
        {
            class ImageComposer
            {
            public:
                /* Red-cyan: the red of the left image, and the green and blue of the right one. False, and an empty image, if their sizes differ. */
                static bool make_anaglyph(const Image& in_left_image,
                                          const Image& in_right_image,
                                          Image*       out_image_ptr);

                /* The images in rows of in_n_columns, left to right, with a gap between them, on dark gray. A cell is as large as the largest image, and an
                 * image is at its top left. False, and an empty image, if there are no images, no columns, or an image is empty.
                 */
                static bool make_contact_sheet(const std::vector<Image>& in_image_vec,
                                               const uint32_t&           in_n_columns,
                                               Image*                    out_image_ptr);

                /* What of the two eyes the view says: one of them, both side by side, or both as an anaglyph. False, and an empty image, if the images cannot be
                 * put together.
                 */
                static bool make_eye_view(const EyeView& in_view,
                                          const Image&   in_left_image,
                                          const Image&   in_right_image,
                                          Image*         out_image_ptr);
            };
        }
    }
}

#endif /* STEREO_REPLAY_FRAMEIMAGES_IMAGE_COMPOSER_H */

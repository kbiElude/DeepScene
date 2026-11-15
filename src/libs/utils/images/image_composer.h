/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Puts images together: the two eyes of a frame as an anaglyph, and many images in a grid.
 */
#ifndef UTILS_IMAGES_IMAGE_COMPOSER_H
#define UTILS_IMAGES_IMAGE_COMPOSER_H

#include "utils/images/types.h"
#include <stdint.h>
#include <vector>

namespace Utils
{
    namespace Images
    {
        class ImageComposer
        {
        public:
            /* Builds a red-cyan anaglyph: red from the left image, green and blue from the right image. Returns false and an empty image if the sizes differ. */
            static bool make_anaglyph(const Image& in_left_image,
                                      const Image& in_right_image,
                                      Image*       out_image_ptr);

            /* Arranges the images in a grid with in_n_columns columns, filled left to right, with a gap between cells, on a dark gray background. Each cell
             * is as large as the largest image, and an image sits at the top left of its cell. Returns false and an empty image if there are no images,
             * no columns, or an image is empty.
             */
            static bool make_contact_sheet(const std::vector<Image>& in_image_vec,
                                           const uint32_t&           in_n_columns,
                                           Image*                    out_image_ptr);

            /* Builds the image the view asks for: one eye, both eyes side by side, or both eyes as an anaglyph. Returns false and an empty image if the
             * images cannot be combined.
             */
            static bool make_eye_view(const EyeView& in_view,
                                      const Image&   in_left_image,
                                      const Image&   in_right_image,
                                      Image*         out_image_ptr);

        private:
            static const size_t N_BYTES_PER_PIXEL;

            /* True if the image is not empty and contains as many pixels as its size says. */
            static bool is_usable(const Image& in_image);
        };
    }
}

#endif /* UTILS_IMAGES_IMAGE_COMPOSER_H */

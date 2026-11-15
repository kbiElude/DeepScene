/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Says how two images differ, and shows it.
 */
#ifndef UTILS_IMAGES_IMAGE_COMPARER_H
#define UTILS_IMAGES_IMAGE_COMPARER_H

#include "utils/images/types.h"

namespace Utils
{
    namespace Images
    {
        class ImageComparer
        {
        public:
            /* Images of different sizes are not compared further: has_same_size is false and all other fields are 0. */
            static ImageDifference compare(const Image& in_first_image,
                                           const Image& in_second_image);

            /* Builds an image of the same size as the inputs, black where the two images match and red where they differ. Returns false and an empty
             * image if the sizes differ.
             */
            static bool make_difference_image(const Image& in_first_image,
                                              const Image& in_second_image,
                                              Image*       out_image_ptr);

        private:
            static const size_t N_BYTES_PER_PIXEL;

            /* True if both images contain as many pixels as their width and height say. */
            static bool are_comparable(const Image& in_first_image,
                                       const Image& in_second_image);
        };
    }
}

#endif /* UTILS_IMAGES_IMAGE_COMPARER_H */

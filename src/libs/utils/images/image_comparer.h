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

#endif /* UTILS_IMAGES_IMAGE_COMPARER_H */

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_ENVIRONMENT_H
#define STEREO_CONVERTERDLL_ENVIRONMENT_H

namespace Stereo
{
    namespace ConverterDLL
    {
        /* What the converter reads from the environment of the process: the switches for a run without the program that shows the images, and for
         * measuring (see the page of the live conversion).
         */
        class Environment
        {
        public:
            /* Reads a number; false if the variable is not there, or is too long to be one. */
            static bool get_number(const char* in_name_ptr,
                                   float*      out_value_ptr);

            /* Whether the variable is there, whatever it holds. */
            static bool is_set(const char* in_name_ptr);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_ENVIRONMENT_H */

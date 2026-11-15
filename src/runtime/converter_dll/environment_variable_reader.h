/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_ENVIRONMENT_VARIABLE_READER_H
#define STEREO_CONVERTERDLL_ENVIRONMENT_VARIABLE_READER_H

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Reads the environment variables of the process. The converter uses them as switches for a run without the viewer (the program that shows
         * the images) and for measuring (see the page of the live conversion).
         */
        class EnvironmentVariableReader
        {
        public:
            /* Reads a number into out_value_ptr. Returns false if the variable does not exist or its text is too long to be a number. */
            static bool get_number(const char* in_name_ptr,
                                   float*      out_value_ptr);

            /* Returns true if the variable exists, whatever its value is. */
            static bool is_set(const char* in_name_ptr);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_ENVIRONMENT_VARIABLE_READER_H */

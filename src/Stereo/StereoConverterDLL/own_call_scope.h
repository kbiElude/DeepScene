/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOCONVERTERDLL_OWN_CALL_SCOPE_H
#define STEREOCONVERTERDLL_OWN_CALL_SCOPE_H

#include <stdint.h>

namespace Stereo
{
    namespace StereoConverterDLL
    {
        /* While one exists on a thread, the calls that the thread makes are the converter's own, not the application's: the callbacks
         * leave them alone, so that what the converter does to the device is not taken for what the application does.
         */
        class OwnCallScope
        {
        public:
            OwnCallScope();
            ~OwnCallScope();

            OwnCallScope(const OwnCallScope&)            = delete;
            OwnCallScope& operator=(const OwnCallScope&) = delete;

            /* True if the calling thread is in a scope. */
            static bool is_active();

        private:
            static uint32_t& get_depth_ref();
        };
    }
}

#endif /* STEREOCONVERTERDLL_OWN_CALL_SCOPE_H */

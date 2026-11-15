/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_OWN_CALL_SCOPE_H
#define STEREO_CONVERTERDLL_OWN_CALL_SCOPE_H

#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* While an OwnCallScope exists on a thread, the calls that the thread makes belong to the converter, not to the application. The callbacks
         * ignore them, so the converter's own use of the device is not mistaken for the application's.
         */
        class OwnCallScope
        {
        public:
             OwnCallScope();
            ~OwnCallScope();

            OwnCallScope           (const OwnCallScope&) = delete;
            OwnCallScope& operator=(const OwnCallScope&) = delete;

            /* Returns true if the calling thread is inside an OwnCallScope. */
            static bool is_active();

        private:
            static uint32_t& get_depth_ref();
        };
    }
}

#endif /* STEREO_CONVERTERDLL_OWN_CALL_SCOPE_H */

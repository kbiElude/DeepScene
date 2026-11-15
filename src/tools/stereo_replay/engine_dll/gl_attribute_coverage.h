/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_GL_ATTRIBUTE_COVERAGE_H
#define STEREO_REPLAY_ENGINEDLL_GL_ATTRIBUTE_COVERAGE_H

#include "common/types.h"
#include <stdint.h>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Tells which calls a glPopAttrib undoes. These are the calls that only assign a value to a piece of state (a capability, a
             * material, a blend function, the current color), grouped by the glPushAttrib attribute group that saves that state. A call
             * that the pop undoes can be left out if nothing reads its effect before the pop. Three kinds of calls are not covered:
             * calls that choose where later calls go (the active texture unit, the matrix mode, the bound texture), calls that change
             * an object (the parameters of a texture), and calls that change client state (the arrays and their pointers, which
             * glPushClientAttrib saves and glPushAttrib does not).
             */
            class GLAttributeCoverage
            {
            public:
                /* Returns the glPushAttrib groups that save the state the call sets, as a mask (GL_ENABLE_BIT, ...). Returns 0 for a call
                 * that a pop does not undo.
                 */
                static uint32_t get_attribute_bits(const APIInterceptor::APIFunction& in_function);

                /* True if a push with this mask saves everything the call sets. */
                static bool is_undone_by_pop(const uint32_t&                    in_mask,
                                             const APIInterceptor::APIFunction& in_function);

            private:
                static uint32_t get_bits_of_name(const char* in_name_ptr);
                static bool     has_prefix      (const char* in_name_ptr,
                                                 const char* in_prefix_ptr);
                static bool     is_digit_after  (const char* in_name_ptr,
                                                 const char* in_prefix_ptr);
                static bool     is_name         (const char* in_name_ptr,
                                                 const char* in_other_ptr);
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_GL_ATTRIBUTE_COVERAGE_H */

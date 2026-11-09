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
            /* Which calls a glPopAttrib undoes: the calls that only give a piece of state a value (a capability, a material, a blend function, the current color), by
             * the attribute group that glPushAttrib saves it in. A call whose effect the pop undoes need not be made, if nothing that is made looks at it before the
             * pop. The calls that choose where the next calls go (the active texture unit, the matrix mode, the bound texture), those that change an object (the
             * parameters of a texture), and the state of the client (the arrays and their pointers, which glPushClientAttrib saves, and not glPushAttrib) are not
             * of them.
             */
            class GlAttributeCoverage
            {
            public:
                /* The groups of glPushAttrib that the call is saved in, as its mask (GL_ENABLE_BIT, ...); 0 for a call that a pop does not undo. */
                static uint32_t get_attribute_bits(const APIInterceptor::APIFunction& in_function);

                /* True if a push of the mask saves all that the call sets. */
                static bool is_undone_by_pop(const uint32_t&                    in_mask,
                                             const APIInterceptor::APIFunction& in_function);
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_GL_ATTRIBUTE_COVERAGE_H */

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_I_EYE_EXECUTOR_H
#define STEREO_REPLAY_ENGINEDLL_I_EYE_EXECUTOR_H

#include "analysis/recipe/types.h"
#include <functional>
#include <stdint.h>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Tells whether an object which the application makes, by the address that the dump gives it, is one that each eye has a copy of (see
             * PerEyeTexture). Asked when the object is made, which is when the copies are.
             */
            typedef std::function<bool(const uint64_t& in_dumped_address)> PerEyePredicate;

            /* An executor of the calls of an API which keeps a copy of some objects for each eye. */
            class IEyeExecutor
            {
            public:
                virtual ~IEyeExecutor() = default;

                /* The eye that the calls which follow are executed for, until the next time: the objects which each eye has a copy of are the ones of
                 * that eye. The left one to begin with, and for what is not rendered for two eyes.
                 */
                virtual void set_eye(const Analysis::Recipe::Eye& in_eye) = 0;

                /* How many times the frame that is about to be played is executed (1 or 2): an executor that keeps a copy of an object for each eye brings the copy of the eye that is
                 * not executed up to date by itself in a frame that is executed once, and leaves it to the other execution in one that is executed twice. Nothing is done
                 * by default.
                 */
                virtual void set_n_executions(const uint32_t& in_n_executions)
                {
                    (void) in_n_executions;
                }

                /* What tells which objects have a copy for each eye; none, if it is not set. */
                virtual void set_per_eye_predicate(const PerEyePredicate& in_predicate) = 0;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_I_EYE_EXECUTOR_H */

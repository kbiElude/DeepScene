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
            /* Tells whether an object the application creates has a separate copy for each eye (see PerEyeTexture). The object is
             * identified by the address the dump gives it. The executor asks when it creates the object, which is also when it
             * creates the copies.
             */
            typedef std::function<bool(const uint64_t& in_dumped_address)> PerEyePredicate;

            /* An executor of the calls of one API that keeps a separate copy of some objects for each eye. */
            class IEyeExecutor
            {
            public:
                virtual ~IEyeExecutor() = default;

                /* Sets the eye that the following calls are executed for, until the next call of set_eye(). Objects that have a copy
                 * per eye resolve to the copy of that eye. The initial eye is the left one, which is also used for frames that are not
                 * rendered for two eyes.
                 */
                virtual void set_eye(const Analysis::Recipe::Eye& in_eye) = 0;

                /* Tells how many times the frame about to be played is executed (1 or 2). An executor that keeps a copy of an object per
                 * eye uses it like this. If the frame is executed once, the executor brings the copy of the eye that is not executed up to
                 * date itself. If the frame is executed twice, the other execution does that. The default implementation does nothing.
                 */
                virtual void set_n_executions(const uint32_t& in_n_executions)
                {
                    (void) in_n_executions;
                }

                /* Sets the predicate that tells which objects have a copy per eye. Without a predicate, no object has one. */
                virtual void set_per_eye_predicate(const PerEyePredicate& in_predicate) = 0;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_I_EYE_EXECUTOR_H */

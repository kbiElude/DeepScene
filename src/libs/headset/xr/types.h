/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The enums of the headset session.
 */
#ifndef HEADSET_XR_TYPES_H
#define HEADSET_XR_TYPES_H

namespace Headset
{
    namespace XR
    {
        /* The states of a swap chain image for one eye. An acquired image is waited for, then copied to, then released. */
        enum class SwapchainImageState
        {
            ACQUIRED, /* acquired from the swap chain; the compositor is still using it */
            IDLE,
            READY     /* the compositor has finished with it: it can be copied to       */
        };
    }
}

#endif /* HEADSET_XR_TYPES_H */

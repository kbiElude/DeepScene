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
        /* What of an image a swap chain gives the eye: an image which has been taken is waited for, then copied to, then given back. */
        enum class SwapchainImageState
        {
            ACQUIRED, /* taken, and the compositor has not said that it is done with it yet */
            IDLE,
            READY     /* the compositor is done with it: it can be copied to              */
        };
    }
}

#endif /* HEADSET_XR_TYPES_H */

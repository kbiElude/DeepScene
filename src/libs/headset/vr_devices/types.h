/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the VR player knows about a device it can play on.
 */
#ifndef HEADSET_VRDEVICES_TYPES_H
#define HEADSET_VRDEVICES_TYPES_H

#include <stdint.h>
#include <string>

namespace Headset
{
    namespace VRDevices
    {
        /* What the device is, as OpenXR tells it (the form factor of a system). */
        enum class FormFactor
        {
            HANDHELD_DISPLAY,
            HEAD_MOUNTED_DISPLAY
        };

        struct VrDevice
        {
            uint32_t    eye_height  = 0;     /* of the image of an eye that the device is meant to be rendered at; 0 if it is not known       */
            uint32_t    eye_width   = 0;
            FormFactor  form_factor = FormFactor::HEAD_MOUNTED_DISPLAY;

            std::string id;                 /* the same in every run, so that a choice can be kept; not shown   */
            std::string name;               /* what is shown                                                    */
            std::string runtime_name;       /* of the OpenXR runtime which serves the device                    */
            std::string unavailable_reason; /* empty if the device can be played on                             */
        };
    }
}

#endif /* HEADSET_VRDEVICES_TYPES_H */

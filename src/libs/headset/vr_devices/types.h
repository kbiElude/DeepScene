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
        /* The kind of device, as OpenXR reports it (the form factor of the system). */
        enum class FormFactor
        {
            HANDHELD_DISPLAY,
            HEAD_MOUNTED_DISPLAY
        };

        struct VrDevice
        {
            uint32_t    eye_height  = 0;     /* the image height of one eye that the device is meant to be rendered at; 0 if unknown           */
            uint32_t    eye_width   = 0;
            FormFactor  form_factor = FormFactor::HEAD_MOUNTED_DISPLAY;

            std::string id;                 /* stable across runs so that a choice can be remembered; not shown to the user  */
            std::string name;               /* the name shown to the user                                                    */
            std::string runtime_name;       /* the name of the OpenXR runtime that serves the device                         */
            std::string unavailable_reason; /* empty if the device can be played on                                          */
        };
    }
}

#endif /* HEADSET_VRDEVICES_TYPES_H */

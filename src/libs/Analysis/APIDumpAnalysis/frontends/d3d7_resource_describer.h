/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the model needs to know of the surfaces and the vertex buffers of DirectDraw 7 and Direct3D 7.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D7_RESOURCE_DESCRIBER_H
#define APIDUMPANALYSIS_FRONTENDS_D3D7_RESOURCE_DESCRIBER_H

#include "Analysis/APIDumpAnalysis/types.h"
#include "Interception/Core/D3D7Utils/value_readers.h"
#include "Interception/Hooks/D3D7/types.h"

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class D3D7ResourceDescriber
            {
            public:
                static Analysis::APIDumpAnalysis::Format convert_pixel_format(const Interception::Hooks::D3D7::DDPIXELFORMAT& in_format);

                /* The back buffer of a flipping chain is made like the front buffer of it, which is the primary surface. */
                static Analysis::APIDumpAnalysis::ResourceDescription describe_back_buffer(const Analysis::APIDumpAnalysis::ResourceDescription& in_primary);

                /* The surface that CreateSurface makes. A surface that has no size of its own (the primary surface) is as large as the screen. */
                static Analysis::APIDumpAnalysis::ResourceDescription describe_surface(const Interception::Core::D3D7Utils::SurfaceDescription& in_description,
                                                                                       const uint32_t&                                          in_screen_width,
                                                                                       const uint32_t&                                          in_screen_height);

                static Analysis::APIDumpAnalysis::ResourceDescription describe_vertex_buffer(const Interception::Hooks::D3D7::D3DVERTEXBUFFERDESC& in_description);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D7_RESOURCE_DESCRIBER_H */

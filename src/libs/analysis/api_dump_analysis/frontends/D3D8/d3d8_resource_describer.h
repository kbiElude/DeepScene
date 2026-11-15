/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Describes a D3D8 resource in the terms of the model, based on the call that creates it. The pool and the usage flags have no general
 * counterpart in the model, so they are stored unchanged in the api_flags of the description.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D8_RESOURCE_DESCRIBER_H
#define APIDUMPANALYSIS_FRONTENDS_D3D8_RESOURCE_DESCRIBER_H

#include "analysis/api_dump_analysis/types.h"
#include "analysis/api_dump_analysis/frontends/shared/call_view.h"
#include "interception/core/D3D8Utils/types.h"

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class D3D8ResourceDescriber
            {
            public:
                static Analysis::APIDumpAnalysis::Format convert_format(const Interception::Hooks::D3D8::D3DFORMAT& in_format);

                /* The depth buffer that the device creates for itself, if the application requests one. */
                static Analysis::APIDumpAnalysis::ResourceDescription describe_auto_depth_stencil(const Interception::Core::D3D8Utils::PresentParameters& in_parameters);

                static Analysis::APIDumpAnalysis::ResourceDescription describe_back_buffer(const Interception::Core::D3D8Utils::PresentParameters& in_parameters);

                /* The describe_* functions below read the arguments of the corresponding CreateXxx methods. */
                static Analysis::APIDumpAnalysis::ResourceDescription describe_cube_texture         (const CallView& in_view);
                static Analysis::APIDumpAnalysis::ResourceDescription describe_depth_stencil_surface(const CallView& in_view);
                static Analysis::APIDumpAnalysis::ResourceDescription describe_image_surface        (const CallView& in_view);
                static Analysis::APIDumpAnalysis::ResourceDescription describe_index_buffer         (const CallView& in_view);
                static Analysis::APIDumpAnalysis::ResourceDescription describe_render_target        (const CallView& in_view);
                static Analysis::APIDumpAnalysis::ResourceDescription describe_texture              (const CallView& in_view);
                static Analysis::APIDumpAnalysis::ResourceDescription describe_vertex_buffer        (const CallView& in_view);
                static Analysis::APIDumpAnalysis::ResourceDescription describe_volume_texture       (const CallView& in_view);

            private:
                /* Combines the pool and the usage flags into the single number that the description stores. */
                static uint64_t compute_api_flags(const Interception::Hooks::D3D8::D3DPOOL& in_pool,
                                                  const uint32_t&                           in_usage);

                /* The part that all 2D surfaces have in common. */
                static Analysis::APIDumpAnalysis::ResourceDescription describe_surface(const uint32_t&                             in_width,
                                                                                       const uint32_t&                             in_height,
                                                                                       const Interception::Hooks::D3D8::D3DFORMAT& in_format,
                                                                                       const uint32_t&                             in_usage,
                                                                                       const Interception::Hooks::D3D8::D3DPOOL&   in_pool);

                static uint32_t get_n_samples(const Interception::Hooks::D3D8::D3DMULTISAMPLE_TYPE& in_type);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D8_RESOURCE_DESCRIBER_H */

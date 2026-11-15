/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The state of the OpenGL fixed-function pipeline that distinguishes one draw from another: what is enabled (blending, depth and stencil tests,
 * culling, lighting, fog, texturing...) and a few settings (the depth and blend functions, the polygon modes...). A setting that has the default
 * value is the same as a setting that was never made, so draws are distinguished only by what the program changed.
 *
 * glPushAttrib saves a copy of this state, and glPopAttrib restores the parts that belong to the groups in its mask.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_PIPELINE_STATE_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_PIPELINE_STATE_H

#include "analysis/api_dump_analysis/frontends/OpenGL/types.h"
#include "analysis/api_dump_analysis/types.h"
#include <map>
#include <set>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLPipelineState
            {
            public:
                /* Describes a draw using the current state. The caller adds the geometry. The OpenGL camera looks down minus z. */
                Analysis::APIDumpAnalysis::DrawDescription describe_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                         const uint32_t&                                     in_vertex_count) const;

                /* Returns true if the capability is enabled for the texture unit. The unit only matters for capabilities that each unit has (see is_unit_capability). */
                bool is_unit_capability_enabled(const uint32_t& in_unit,
                                                const uint32_t& in_capability) const;

                /* Restores the parts of the state that belong to the groups of the mask (a mask with all bits set restores everything). */
                void restore(const GLPipelineState& in_saved,
                             const uint32_t&        in_mask);

                void set_capability(const uint32_t& in_capability,
                                    const bool&     in_is_enabled);

                /* Applies to the capabilities that glEnable sets for the active texture unit. */
                void set_unit_capability(const uint32_t& in_unit,
                                         const uint32_t& in_capability,
                                         const bool&     in_is_enabled);

                void set_value(const GLStateValue& in_value,
                               const uint32_t&     in_setting);

                /* Returns true for the capabilities that each texture unit has separately: the texture targets and the generation of coordinates. */
                static bool is_unit_capability(const uint32_t& in_capability);

            private:
                uint64_t compute_state_key() const;

                void set_key(const uint64_t& in_key,
                             const bool&     in_is_enabled);

                /* The bits of the attribute groups that contain the capability, as used in the mask of glPushAttrib. */
                static uint32_t get_capability_groups(const uint32_t& in_capability);

                /* The key of a capability in the enabled set. For a texture unit capability it combines the unit and the capability. For any other capability the key is the capability itself. */
                static uint64_t get_unit_key(const uint32_t& in_unit,
                                             const uint32_t& in_capability);

                static uint32_t get_value_groups(const GLStateValue& in_value);

                std::set<uint64_t>               m_enabled_capability_set;
                std::map<GLStateValue, uint32_t> m_value_map;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_PIPELINE_STATE_H */

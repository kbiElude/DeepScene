/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The state of the fixed-function pipeline of OpenGL that a draw is told apart by: what is enabled (blending, the tests of the depth and the
 * stencil, culling, lighting, fog, texturing...) and a few settings (the functions of the depth and of the blending, the modes of the
 * polygons...). A setting that is the default is the same as one never made, so a draw is told by what the program changed.
 *
 * glPushAttrib saves a copy of it, and glPopAttrib puts back the parts that the groups of its mask hold.
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
                /* A draw made with what is set now, but for the geometry, which the caller adds. The camera of OpenGL looks down minus z. */
                Analysis::APIDumpAnalysis::DrawDescription describe_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                         const uint32_t&                                     in_vertex_count) const;

                /* True if the capability is enabled for the texture unit; for the capabilities that are the unit's own (see is_unit_capability). */
                bool is_unit_capability_enabled(const uint32_t& in_unit,
                                                const uint32_t& in_capability) const;

                /* Puts back what the groups of the mask hold (a mask of all the bits puts back all of it). */
                void restore(const GLPipelineState& in_saved,
                             const uint32_t&        in_mask);

                void set_capability(const uint32_t& in_capability,
                                    const bool&     in_is_enabled);

                /* For the capabilities that glEnable sets for the active texture unit. */
                void set_unit_capability(const uint32_t& in_unit,
                                         const uint32_t& in_capability,
                                         const bool&     in_is_enabled);

                void set_value(const GLStateValue& in_value,
                               const uint32_t&     in_setting);

                /* True for the capabilities that each texture unit has its own of: the targets of the textures, and the generation of coordinates. */
                static bool is_unit_capability(const uint32_t& in_capability);

            private:
                uint64_t compute_state_key() const;

                void set_key(const uint64_t& in_key,
                             const bool&     in_is_enabled);

                /* The bits of the attribute groups that hold the capability, which glPushAttrib takes a mask of. */
                static uint32_t get_capability_groups(const uint32_t& in_capability);

                /* The key of a capability of a texture unit among the enabled ones; that of any other capability is the capability. */
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

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Makes up the accesses of a workload for the tests of the dependencies: resources, and what is done to them call by call, frame by frame.
 */
#ifndef TESTS_DEPENDENCY_SCENE_H
#define TESTS_DEPENDENCY_SCENE_H

#include "analysis/api_dump_dependencies/dependency_analyzer.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Tests
{
    /* Accesses are made one call at a time (all of those of a call share its number), and frames end where they are told to. */
    class DependencyScene
    {
    public:
        void access(const Analysis::APIDumpAnalysis::ResourceId&       in_id,
                    const Analysis::APIDumpAnalysis::AccessRole&       in_role,
                    const Analysis::APIDumpAnalysis::AccessMode&       in_mode,
                    const Analysis::APIDumpAnalysis::AccessCoverage&   in_coverage = Analysis::APIDumpAnalysis::AccessCoverage::UNKNOWN,
                    const Analysis::APIDumpAnalysis::SubresourceRange& in_range    = Analysis::APIDumpAnalysis::SubresourceRange() )
        {
            Analysis::APIDumpAnalysis::AccessEvent event;

            event.call_index        = m_call_index;
            event.coverage          = in_coverage;
            event.mode              = in_mode;
            event.resource_id       = in_id;
            event.role              = in_role;
            event.subresource_range = in_range;

            m_log.on_access(event);
        }

        Analysis::APIDumpDependencies::DependencyResult analyze(const uint32_t& in_n_threads = 1) const
        {
            return Analysis::APIDumpDependencies::DependencyAnalyzer::analyze(m_log,
                                                                              m_resource_vec,
                                                                              in_n_threads);
        }

        /* A texture which is cleared (all of it). */
        void clear_fully(const Analysis::APIDumpAnalysis::ResourceId& in_id)
        {
            access(in_id,
                   Analysis::APIDumpAnalysis::AccessRole::CLEAR_TARGET,
                   Analysis::APIDumpAnalysis::AccessMode::WRITE,
                   Analysis::APIDumpAnalysis::AccessCoverage::FULL);
            next_call();
        }

        Analysis::APIDumpAnalysis::ResourceId create_resource(const uint32_t&                            in_n_mip_levels = 1,
                                                              const Analysis::APIDumpAnalysis::ResourceKind& in_kind         = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D,
                                                              const bool&                                in_is_render_target = false,
                                                              const uint64_t&                            in_match_key    = 0)
        {
            Analysis::APIDumpAnalysis::Resource resource;

            resource.debug_name                    = "Surface#" + std::to_string(m_resource_vec.size() );
            resource.description.height            = 64;
            resource.description.is_render_target  = in_is_render_target;
            resource.description.kind              = in_kind;
            resource.description.mip_level_count   = in_n_mip_levels;
            resource.description.width             = 128;
            resource.id                            = Analysis::APIDumpAnalysis::make_resource_id(static_cast<uint32_t>(m_resource_vec.size() ) + 1);
            resource.match_key                     = in_match_key;

            m_resource_vec.push_back(resource);

            return resource.id;
        }

        /* A draw which writes where it reaches, which is not known. */
        void draw_to(const Analysis::APIDumpAnalysis::ResourceId& in_id)
        {
            access(in_id,
                   Analysis::APIDumpAnalysis::AccessRole::COLOR_ATTACHMENT,
                   Analysis::APIDumpAnalysis::AccessMode::WRITE);
            next_call();
        }

        void end_frame()
        {
            m_log.on_frame_ended(m_n_frames_ended++,
                                 m_first_call_index,
                                 m_call_index);

            m_call_index++;

            m_first_call_index = m_call_index;
        }

        /* What a .deps file would hold of the workload that was made up. */
        Analysis::APIDumpDependencies::DependencyAnalysis make_analysis(const uint32_t& in_n_threads = 1) const
        {
            Analysis::APIDumpDependencies::DependencyAnalysis result;

            result.frame_call_range_vec        = m_log.get_frame_call_range_vec();
            result.n_calls                     = m_call_index;
            result.n_calls_by_api_map["d3d7"]  = m_call_index;
            result.pointer_n_bytes             = 4;
            result.qpc_frequency               = 10000000;
            result.result                      = analyze(in_n_threads);
            result.resource_vec                = m_resource_vec;

            result.workload_reference.filename = "x.workload";
            result.workload_reference.n_bytes  = 1000;

            return result;
        }

        const Analysis::APIDumpDependencies::AccessLog& get_log() const
        {
            return m_log;
        }

        /* The next call: what is accessed from now on is accessed by another call. */
        void next_call()
        {
            m_call_index++;
        }

        void sample(const Analysis::APIDumpAnalysis::ResourceId& in_id)
        {
            access(in_id,
                   Analysis::APIDumpAnalysis::AccessRole::SAMPLED,
                   Analysis::APIDumpAnalysis::AccessMode::READ);
            next_call();
        }

    private:
        uint64_t                                          m_call_index       = 0;
        uint64_t                                          m_first_call_index = 0;
        Analysis::APIDumpDependencies::AccessLog          m_log;
        uint64_t                                          m_n_frames_ended   = 0;
        std::vector<Analysis::APIDumpAnalysis::Resource>  m_resource_vec;
    };
}

#endif /* TESTS_DEPENDENCY_SCENE_H */

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpDependencies/per_eye_candidates.h"

std::vector<Analysis::APIDumpDependencies::PerEyeCandidate> Analysis::APIDumpDependencies::PerEyeCandidates::find(const DependencyAnalysis& in_analysis)
{
    std::vector<PerEyeCandidate> result_vec;

    for (size_t n_resource = 0;
                n_resource < in_analysis.resource_vec.size()  &&
                n_resource < in_analysis.result.resource_vec.size();
              ++n_resource)
    {
        const auto&            resource       = in_analysis.resource_vec.at(n_resource);
        const ResourceSummary& summary        = in_analysis.result.resource_vec.at(n_resource).summary;
        const bool             is_rendered_to = (resource.description.is_render_target ||
                                                 resource.description.is_depth_stencil);

        /* Some frame uses what an earlier frame left in it, whether that frame writes it too or not. */
        const bool is_used_later = (summary.n_frames_using_earlier_content          != 0 ||
                                    summary.n_frames_using_earlier_content_possibly != 0);

        if (is_used_later                                                                         &&
            is_rendered_to                                                                        &&
            resource.description.kind != Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE)
        {
            PerEyeCandidate candidate;

            candidate.is_certain = (summary.n_frames_using_earlier_content != 0);
            candidate.n_resource = n_resource;

            result_vec.push_back(candidate);
        }
    }

    return result_vec;
}

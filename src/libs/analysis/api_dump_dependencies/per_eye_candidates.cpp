/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_dependencies/per_eye_candidates.h"
#include <algorithm>
#include <map>
#include <utility>

namespace
{
    /* Whether the resource is one that a frame renders to, and which is not the image of a swap chain. */
    bool is_rendered_to_by_frames(const Analysis::APIDumpAnalysis::Resource& in_resource)
    {
        return ((in_resource.description.is_render_target || in_resource.description.is_depth_stencil) &&
                in_resource.description.kind != Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE);
    }
}

std::vector<Analysis::APIDumpDependencies::PerEyeCandidate> Analysis::APIDumpDependencies::PerEyeCandidates::find(const DependencyAnalysis& in_analysis)
{
    const auto                   group_vec = find_rotation_groups(in_analysis);
    std::map<size_t, size_t>     group_by_resource_map; /* the number of the resource less 1, and its group */
    std::vector<PerEyeCandidate> result_vec;

    for (size_t n_group = 0;
                n_group < group_vec.size();
              ++n_group)
    {
        for (const size_t n_resource : group_vec.at(n_group).n_resource_vec)
        {
            group_by_resource_map[n_resource] = n_group;
        }
    }

    for (size_t n_resource = 0;
                n_resource < in_analysis.resource_vec.size()  &&
                n_resource < in_analysis.result.resource_vec.size();
              ++n_resource)
    {
        const auto&            resource       = in_analysis.resource_vec.at(n_resource);
        const ResourceSummary& summary        = in_analysis.result.resource_vec.at(n_resource).summary;
        const bool             is_rendered_to = is_rendered_to_by_frames(resource);

        /* Some frame uses what an earlier frame left in it, whether that frame writes it too or not. */
        const bool is_used_later = (summary.n_frames_using_earlier_content          != 0 ||
                                    summary.n_frames_using_earlier_content_possibly != 0);

        if (is_used_later &&
            is_rendered_to)
        {
            PerEyeCandidate candidate;

            candidate.is_certain = (summary.n_frames_using_earlier_content != 0);
            candidate.n_resource = n_resource;

            result_vec.push_back(candidate);
        }
    }

    /* A rotation group is one thing: if the frames use earlier content of one of its resources, they do of all of them for the eyes. */
    for (size_t n_group = 0;
                n_group < group_vec.size();
              ++n_group)
    {
        const auto& group         = group_vec.at(n_group);
        const bool  has_candidate = std::any_of(result_vec.begin(),
                                                result_vec.end  (),
                                                [&](const PerEyeCandidate& in_candidate)
                                                {
                                                    const auto iterator = group_by_resource_map.find(in_candidate.n_resource);

                                                    return (iterator != group_by_resource_map.end() &&
                                                            iterator->second == n_group);
                                                });

        if (has_candidate)
        {
            for (const size_t n_resource : group.n_resource_vec)
            {
                const bool is_listed = std::any_of(result_vec.begin(),
                                                   result_vec.end  (),
                                                   [&](const PerEyeCandidate& in_candidate)
                                                   {
                                                       return (in_candidate.n_resource == n_resource);
                                                   });

                if (is_listed == false)
                {
                    PerEyeCandidate candidate;

                    candidate.is_added_by_group = true;
                    candidate.n_resource        = n_resource;

                    result_vec.push_back(candidate);
                }
            }
        }
    }

    std::sort(result_vec.begin(),
              result_vec.end  (),
              [](const PerEyeCandidate& in_a,
                 const PerEyeCandidate& in_b)
              {
                  return (in_a.n_resource < in_b.n_resource);
              });

    for (auto& candidate : result_vec)
    {
        const auto iterator = group_by_resource_map.find(candidate.n_resource);

        if (iterator != group_by_resource_map.end() )
        {
            candidate.rotation_group_index = iterator->second;
        }
    }

    return result_vec;
}

std::vector<Analysis::APIDumpDependencies::RotationGroup> Analysis::APIDumpDependencies::PerEyeCandidates::find_rotation_groups(const DependencyAnalysis& in_analysis)
{
    constexpr size_t N_MIN_WRITING_FRAMES = 2;

    std::map<uint64_t, std::vector<size_t> > member_vec_by_key_map; /* by what the resources are alike in */
    std::vector<RotationGroup>               result_vec;

    for (size_t n_resource = 0;
                n_resource < in_analysis.resource_vec.size()  &&
                n_resource < in_analysis.result.resource_vec.size();
              ++n_resource)
    {
        const auto& resource = in_analysis.resource_vec.at(n_resource);

        if (is_rendered_to_by_frames(resource)                                                              &&
            in_analysis.result.resource_vec.at(n_resource).summary.n_frames_written >= N_MIN_WRITING_FRAMES)
        {
            member_vec_by_key_map[resource.match_key].push_back(n_resource);
        }
    }

    for (const auto& key_members : member_vec_by_key_map)
    {
        const auto&                               member_vec = key_members.second;
        bool                                      has_repeat = false;
        std::vector<std::pair<uint64_t, size_t> > turn_vec; /* the frame of a write, and the member that wrote it */

        if (member_vec.size() < 2)
        {
            continue;
        }

        for (size_t n_member = 0;
                    n_member < member_vec.size();
                  ++n_member)
        {
            for (const auto& version : in_analysis.result.resource_vec.at(member_vec.at(n_member) ).version_vec)
            {
                if (version.has_writer)
                {
                    turn_vec.push_back({version.writer_frame_index, n_member});
                }
            }
        }

        std::sort(turn_vec.begin(),
                  turn_vec.end  () );

        /* Turns, not a run of one resource: no member is written by two writes in a row. */
        for (size_t n_turn = 1;
                    n_turn < turn_vec.size() && has_repeat == false;
                  ++n_turn)
        {
            has_repeat = (turn_vec.at(n_turn).second == turn_vec.at(n_turn - 1).second);
        }

        if (has_repeat           == false &&
            turn_vec.empty()     == false)
        {
            RotationGroup group;

            group.n_resource_vec    = member_vec;
            group.has_regular_order = (turn_vec.size() >= member_vec.size() );

            /* Each member once in the first turns, and the same order from then on. */
            for (size_t n_turn = 0;
                        n_turn < turn_vec.size() && group.has_regular_order;
                      ++n_turn)
            {
                if (n_turn < member_vec.size() )
                {
                    for (size_t n_before = 0;
                                n_before < n_turn;
                              ++n_before)
                    {
                        if (turn_vec.at(n_before).second == turn_vec.at(n_turn).second)
                        {
                            group.has_regular_order = false;
                        }
                    }
                }
                else
                if (turn_vec.at(n_turn).second != turn_vec.at(n_turn - member_vec.size() ).second)
                {
                    group.has_regular_order = false;
                }
            }

            result_vec.push_back(group);
        }
    }

    std::sort(result_vec.begin(),
              result_vec.end  (),
              [](const RotationGroup& in_a,
                 const RotationGroup& in_b)
              {
                  return (in_a.n_resource_vec.front() < in_b.n_resource_vec.front() );
              });

    return result_vec;
}

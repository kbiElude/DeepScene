/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Frame dependencies of a workload: per resource, how each frame uses the content earlier frames left in it. API-independent; built from
 * the accesses the model builder reports (see Analysis::APIDumpAnalysis::AccessEvent).
 */
#ifndef APIDUMPDEPENDENCIES_TYPES_H
#define APIDUMPDEPENDENCIES_TYPES_H

#include "analysis/api_dump_analysis/types.h"
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        /* Whether a frame uses the content that earlier frames left in a resource. */
        enum class CarryKind
        {
            CERTAIN,  /* a read, or a blending or testing write, comes before any write that replaced all content */
            NONE,     /* every read follows a full replacement, or there is no read                                */
            POSSIBLE  /* a read follows only a partial write (or the extent is unknown): old content may show      */
        };

        /* How the frames of a workload use a resource, taken as a whole. */
        enum class DependencyClass
        {
            CARRIED,          /* a frame uses earlier content and also writes the resource, so the next frame uses what it left */
            CARRIED_POSSIBLY, /* the same, but the use is only possible (CarryKind::POSSIBLE)                                   */
            FRAME_LOCAL,      /* no frame uses content left by an earlier frame                                                 */
            STATIC,           /* frames use earlier content but none writes it: written, then only read                         */
            UNUSED
        };

        /* The calls of a frame. Both indices are inclusive. */
        struct FrameCallRange
        {
            uint64_t first_call_index = 0;
            uint64_t last_call_index  = 0;
        };

        /* A run of consecutive frames. Both indices are inclusive. */
        struct FrameRange
        {
            uint64_t first_frame_index = 0;
            uint64_t last_frame_index  = 0;
        };

        /* Slicing result for a target frame (see FrameSlicePlanner). Live frames must be made so frames from the target on come out as in a full
         * run; a dead frame feeds nothing from the target on, so a replay may skip its draws and copies.
         */
        struct FrameSlice
        {
            std::vector<bool> is_live_vec;                 /* by frame index; frames past the end are live */
            uint64_t          n_live_frames      = 0;
            uint64_t          target_frame_index = 0;
        };

        /* What one frame does to one resource. */
        struct FrameUse
        {
            CarryKind carry            = CarryKind::NONE;
            bool      is_fully_written = false; /* a write replaces all content                                     */
            bool      is_read          = false; /* any read: sampled, copied from, blended or tested               */
            bool      is_self_sampled  = false; /* a call samples the resource while it renders to it              */
            bool      is_written       = false;
        };

        /* A run of consecutive frames that use the resource in the same way. A frame that does not touch the resource ends the run. */
        struct UseRun
        {
            uint64_t first_frame_index = 0;
            uint64_t last_frame_index  = 0;
            FrameUse use;
        };

        /* The content a resource holds from the frame that wrote it until the next writer; the frames in between use it. A version without a
         * writer is the content from the resource's creation.
         *
         * input_resource_vec lists the resources (numbers from 1, not this one) whose earlier content flows into this version through the
         * writer's frame: what the writer needs for this version to come out unchanged. has_inputs is true only for a resource with a written
         * extent whose own earlier content does not flow into its writes (a partial write that leaves content in place does not count);
         * otherwise the writer is assumed to need everything it uses.
         */
        struct ResourceVersion
        {
            std::vector<FrameRange> consumer_range_vec;                    /* the frames that use the content, in order                              */
            bool                    has_inputs                 = false;    /* input_resource_vec is valid                                            */
            bool                    has_writer                 = false;
            std::vector<uint32_t>   input_resource_vec;                    /* see the comment of the struct                                          */
            bool                    keeps_earlier_content      = false;    /* the writer replaced only part, so the version before still shows       */
            uint64_t                n_consumer_frames          = 0;
            uint64_t                n_possible_consumer_frames = 0;        /* of n_consumer_frames, those that are POSSIBLE                          */
            uint64_t                writer_frame_index         = 0;        /* valid if has_writer                                                    */

            /* Rectangles the writer wrote; filled only if the extent of all writes is known (see ResourceDependencies::written_extent). */
            std::vector<Analysis::APIDumpAnalysis::AccessRectangle> written_rectangle_vec;
        };

        /* A resource in numbers; a frame counts once per thing it does. */
        struct ResourceSummary
        {
            DependencyClass dependency_class                        = DependencyClass::UNUSED;
            uint64_t        first_frame_index                       = 0; /* valid if n_frames_used is not 0                                                 */
            uint64_t        last_frame_index                        = 0;
            uint64_t        longest_carry_n_frames                  = 0; /* longest distance in frames from a version's writer to a user of it             */
            uint64_t        n_frames_carrying                       = 0; /* frames that use earlier content (CERTAIN) and write the resource               */
            uint64_t        n_frames_carrying_possibly              = 0; /* the same for POSSIBLE                                                          */
            uint64_t        n_frames_read                           = 0;
            uint64_t        n_frames_self_sampled                   = 0;
            uint64_t        n_frames_used                           = 0;
            uint64_t        n_frames_using_earlier_content          = 0; /* frames with a CERTAIN use, written or not */
            uint64_t        n_frames_using_earlier_content_possibly = 0;
            uint64_t        n_frames_written                        = 0;
        };

        struct ResourceDependencies
        {
            ResourceSummary              summary;
            std::vector<UseRun>          use_run_vec;
            std::vector<ResourceVersion> version_vec;

            /* Smallest rectangle (first level, first layer) containing every partial write. Known only if every such write reported where it
             * landed; outside it the resource holds its creation content or that of a full replacement. If unknown, versions carry no rectangles.
             */
            Analysis::APIDumpAnalysis::AccessRectangle written_extent;
        };

        /* How the frames of a workload use every resource. resource_vec is indexed by resource number minus 1. */
        struct DependencyResult
        {
            uint64_t                          n_frames = 0; /* frames that ended, plus one if accesses follow the last end */
            std::vector<ResourceDependencies> resource_vec;
        };

        /* The content of a .deps file: the result plus what identifies it (workload, resources, calls per frame). */
        struct DependencyFileContent
        {
            std::vector<FrameCallRange>                      frame_call_range_vec;       /* of the frames that ended                     */
            bool                                             is_truncated      = false;  /* the workload ends in the middle of a segment */
            uint64_t                                         n_calls           = 0;
            std::map<std::string, uint64_t>                  n_calls_by_api_map;         /* by API name (see APIGroup)                   */
            uint32_t                                         pointer_n_bytes   = 0;      /* pointer size of the dumped process            */
            uint64_t                                         qpc_frequency     = 0;      /* ticks per second                             */
            DependencyResult                                 result;
            std::vector<Analysis::APIDumpAnalysis::Resource> resource_vec;
            Analysis::APIDumpAnalysis::WorkloadReference     workload_reference;
        };

        bool operator==(const FrameUse& in_a,
                        const FrameUse& in_b);
    }
}

#endif /* APIDUMPDEPENDENCIES_TYPES_H */

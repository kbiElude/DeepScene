/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The vocabulary for the dependencies between the frames of a workload: for each resource, how each frame uses the content that earlier frames left
 * in it. Nothing here depends on a graphics API. The types are built from the accesses that the model builder reports (see
 * Analysis::APIDumpAnalysis::AccessEvent).
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
            CERTAIN,  /* a read, or a write that blends or tests, reaches the resource before any write replaced all of its content */
            NONE,     /* every read comes after a write that replaced all of the content, or there is no read                       */
            POSSIBLE  /* only part of it was written before a read (or the extent is unknown): old content may show                 */
        };

        /* How the frames of a workload use a resource, taken as a whole. */
        enum class DependencyClass
        {
            CARRIED,          /* a frame uses content left by earlier frames and also writes the resource, so the next frame uses what it left */
            CARRIED_POSSIBLY, /* the same, but the use of old content is only possible (CarryKind::POSSIBLE)                                   */
            FRAME_LOCAL,      /* no frame uses content that an earlier frame left in the resource                                              */
            STATIC,           /* frames use content left by earlier frames, but none of them also writes it: written, then only read           */
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

        /* The result of slicing for a target frame (see FrameSlicePlanner). A live frame has to be made so that the frames from the target on come
         * out as they do when every frame is made. A dead frame is not needed: nothing that a frame from the target on reads comes from it, so a
         * replay may leave out its draws and copies.
         */
        struct FrameSlice
        {
            std::vector<bool> is_live_vec;                 /* by frame index; frames beyond the end of the vector are live */
            uint64_t          n_live_frames      = 0;
            uint64_t          target_frame_index = 0;
        };

        /* What one frame does to one resource. */
        struct FrameUse
        {
            CarryKind carry            = CarryKind::NONE;
            bool      is_fully_written = false; /* a write replaces all of the content                             */
            bool      is_read          = false; /* any read: sampled, copied from, or read by a blend or a test    */
            bool      is_self_sampled  = false; /* a call samples the resource as a texture while it renders to it */
            bool      is_written       = false;
        };

        /* A run of consecutive frames that use the resource in the same way. A frame that does not touch the resource ends the run. */
        struct UseRun
        {
            uint64_t first_frame_index = 0;
            uint64_t last_frame_index  = 0;
            FrameUse use;
        };

        /* The content that a resource holds from the frame that wrote it until the next frame that writes it. The frames in between use this
         * content. A version without a writer is the content the resource held when the application created it.
         *
         * input_resource_vec lists the resources (by number, from 1) whose content from before the writer's frame flows into this version through
         * the calls of that frame. It does not list this resource itself. It says what the writer must be given for this version to come out
         * unchanged, as opposed to everything the frame uses. has_inputs is true only for a resource that has a written extent and whose own earlier
         * content does not flow into what the frame writes to it (a partial write that leaves the content in place does not count). Otherwise the
         * writer is assumed to need everything it uses.
         */
        struct ResourceVersion
        {
            std::vector<FrameRange> consumer_range_vec;                    /* the frames that use the content, in order                              */
            bool                    has_inputs                 = false;    /* input_resource_vec is valid                                            */
            bool                    has_writer                 = false;
            std::vector<uint32_t>   input_resource_vec;                    /* see the comment of the struct                                          */
            bool                    keeps_earlier_content      = false;    /* the writer replaced only part of it, so the version before still shows */
            uint64_t                n_consumer_frames          = 0;
            uint64_t                n_possible_consumer_frames = 0;        /* of n_consumer_frames, those that may not use the content (POSSIBLE)    */
            uint64_t                writer_frame_index         = 0;        /* valid if has_writer                                                    */

            /* The rectangles that the writer wrote (see ResourceDependencies::written_extent). Filled only if the extent of all writes to the resource
             * is known.
             */
            std::vector<Analysis::APIDumpAnalysis::AccessRectangle> written_rectangle_vec;
        };

        /* A resource in numbers. Each frame is counted once for each thing it does. */
        struct ResourceSummary
        {
            DependencyClass dependency_class                        = DependencyClass::UNUSED;
            uint64_t        first_frame_index                       = 0; /* valid if n_frames_used is not 0                                                */
            uint64_t        last_frame_index                        = 0;
            uint64_t        longest_carry_n_frames                  = 0; /* the longest distance in frames from a version's writer to a frame that uses it */
            uint64_t        n_frames_carrying                       = 0; /* frames that use earlier content (CERTAIN) and write the resource               */
            uint64_t        n_frames_carrying_possibly              = 0; /* the same for POSSIBLE                                                          */
            uint64_t        n_frames_read                           = 0;
            uint64_t        n_frames_self_sampled                   = 0;
            uint64_t        n_frames_used                           = 0;
            uint64_t        n_frames_using_earlier_content          = 0; /* frames with a CERTAIN use, whether or not they write the resource */
            uint64_t        n_frames_using_earlier_content_possibly = 0;
            uint64_t        n_frames_written                        = 0;
        };

        struct ResourceDependencies
        {
            ResourceSummary              summary;
            std::vector<UseRun>          use_run_vec;
            std::vector<ResourceVersion> version_vec;

            /* The smallest rectangle, in the first level of the first layer, that contains every part of the resource reached by a write that does
             * not replace the whole resource. It is known only if every such write reported where it landed. Outside the rectangle the resource then
             * holds the content it was created with, or the content of a write that replaced all of it, whichever frame was made last. If the
             * extent is unknown, the versions carry no rectangles.
             */
            Analysis::APIDumpAnalysis::AccessRectangle written_extent;
        };

        /* How the frames of a workload use every resource. resource_vec is indexed by resource number minus 1. */
        struct DependencyResult
        {
            uint64_t                          n_frames = 0; /* the frames that ended, plus one if accesses follow the last frame end */
            std::vector<ResourceDependencies> resource_vec;
        };

        /* The content of a .deps file: the dependency result, plus what identifies it (the workload, the resources, the calls of each frame). */
        struct DependencyFileContent
        {
            std::vector<FrameCallRange>                      frame_call_range_vec;       /* of the frames that ended                     */
            bool                                             is_truncated      = false;  /* the workload ends in the middle of a segment */
            uint64_t                                         n_calls           = 0;
            std::map<std::string, uint64_t>                  n_calls_by_api_map;         /* by API name (see APIGroup)                   */
            uint32_t                                         pointer_n_bytes   = 0;      /* pointer size of the process that was dumped  */
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

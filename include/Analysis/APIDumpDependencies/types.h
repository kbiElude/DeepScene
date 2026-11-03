/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The vocabulary of the dependencies between the frames of a workload: for each resource, what each frame does with what the frames before it left in
 * it. Nothing here knows about any API; it is made of the accesses that the model builder tells (see Analysis::APIDumpAnalysis::AccessEvent).
 */
#ifndef APIDUMPDEPENDENCIES_TYPES_H
#define APIDUMPDEPENDENCIES_TYPES_H

#include "Analysis/APIDumpAnalysis/types.h"
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        /* Whether a frame uses what the frames before it left in a resource. */
        enum class CarryKind
        {
            CERTAIN,  /* a read, or a write which blends or tests, reaches the resource before anything has replaced all of its content        */
            NONE,     /* the first thing that is done to it replaces all of it, or all that is done to it is a write that nobody reads         */
            POSSIBLE  /* only part of it was written before it is read (or the extent of the write is not known): the old content may show   */
        };

        /* What the frames of a workload do to a resource, as a whole. */
        enum class DependencyClass
        {
            CARRIED,          /* some frame uses what the frames before it left in the resource, and writes it: the next frame will use what that one left     */
            CARRIED_POSSIBLY, /* the same, but only as far as the old content may show (CarryKind::POSSIBLE)                                                 */
            FRAME_LOCAL,      /* no frame uses what an earlier one left in it                                                                                   */
            STATIC,           /* what the frames use was left by frames which do not use it themselves: written, and then only read                           */
            UNUSED
        };

        /* The calls of a frame, from the first to the last, both of them in it. */
        struct FrameCallRange
        {
            uint64_t first_call_index = 0;
            uint64_t last_call_index  = 0;
        };

        /* A run of frames, from the first to the last, both of them in it. */
        struct FrameRange
        {
            uint64_t first_frame_index = 0;
            uint64_t last_frame_index  = 0;
        };

        /* What one frame does to one resource. */
        struct FrameUse
        {
            CarryKind carry            = CarryKind::NONE;
            bool      is_fully_written = false; /* something replaces all of the content                                          */
            bool      is_read          = false; /* sampled, copied from, or read by a blend or a test: any read of the resource    */
            bool      is_self_sampled  = false; /* a call reads the resource as a texture while it renders to it                   */
            bool      is_written       = false;
        };

        /* The frames from the first to the last which do the same to the resource. Frames of the run are all used: a frame which does not touch
         * the resource ends a run.
         */
        struct UseRun
        {
            uint64_t first_frame_index = 0;
            uint64_t last_frame_index  = 0;
            FrameUse use;
        };

        /* What a resource held from the frame which wrote it to the next one which does: the content that the frames in between use. A version
         * which has no writer is what the resource held when the application made it.
         */
        struct ResourceVersion
        {
            std::vector<FrameRange> consumer_range_vec;                    /* the frames which use the content, in order                                 */
            bool                    has_writer                 = false;
            bool                    keeps_earlier_content      = false;    /* the writer did not replace all of it: the version before is still there in part */
            uint64_t                n_consumer_frames          = 0;
            uint64_t                n_possible_consumer_frames = 0;        /* of the above, those which may not use it at all (CarryKind::POSSIBLE)      */
            uint64_t                writer_frame_index         = 0;        /* valid if has_writer                                                        */
        };

        /* A resource in numbers. A frame is counted for what it does, once. */
        struct ResourceSummary
        {
            DependencyClass dependency_class                        = DependencyClass::UNUSED;
            uint64_t        first_frame_index                       = 0; /* valid if n_frames_used is not 0                                           */
            uint64_t        last_frame_index                        = 0;
            uint64_t        longest_carry_n_frames                  = 0; /* from the frame which wrote what a frame uses, to the frame which uses it  */
            uint64_t        n_frames_carrying                       = 0; /* use what the frames before left (CERTAIN), and write the resource         */
            uint64_t        n_frames_carrying_possibly              = 0; /* the same, POSSIBLE                                                        */
            uint64_t        n_frames_read                           = 0;
            uint64_t        n_frames_self_sampled                   = 0;
            uint64_t        n_frames_used                           = 0;
            uint64_t        n_frames_using_earlier_content          = 0; /* CERTAIN, whether they write it or not                                     */
            uint64_t        n_frames_using_earlier_content_possibly = 0;
            uint64_t        n_frames_written                        = 0;
        };

        struct ResourceDependencies
        {
            ResourceSummary              summary;
            std::vector<UseRun>          use_run_vec;
            std::vector<ResourceVersion> version_vec;
        };

        /* What the frames of a workload do with every resource, by the number of the resource less 1. */
        struct DependencyResult
        {
            uint64_t                          n_frames = 0; /* the frames which ended, and one more if accesses follow the last */
            std::vector<ResourceDependencies> resource_vec;
        };

        bool operator==(const FrameUse& in_a,
                        const FrameUse& in_b);
    }
}

#endif /* APIDUMPDEPENDENCIES_TYPES_H */

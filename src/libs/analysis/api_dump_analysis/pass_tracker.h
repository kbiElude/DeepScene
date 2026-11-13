/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Keeps the pass that the model builder is making, and completes it. A pass starts with the first draw or clear after the previous pass ended, and ends when the
 * builder says so (see ModelBuilder); the tracker does not decide that. When a pass is completed, its structure is interned in the model (what it renders to and
 * how, the draws it is made of and the constants in effect), what it rendered to is recorded as written, and its instance, with what is kept for its frame, is added
 * to the frame that is in progress.
 */
#ifndef APIDUMPANALYSIS_PASS_TRACKER_H
#define APIDUMPANALYSIS_PASS_TRACKER_H

#include "analysis/api_dump_analysis/model.h"
#include "analysis/api_dump_analysis/resource_key_table.h"
#include "analysis/api_dump_analysis/types.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class PassTracker
        {
        public:
            PassTracker();

            /* Interns the open pass in the model, records what it rendered to as written, adds its instance to the frame, and leaves no pass open. The camera series
             * and the histograms of the pass are kept only with the history. A pass must be open (see has_open_pass()).
             */
            void complete(const uint64_t&         in_call_index,
                          const bool&             in_keeps_history,
                          const ConstantStateMap& in_constant_state_map,
                          ResourceKeyTable*       inout_key_table_ptr,
                          Model*                  inout_model_ptr,
                          FrameInProgress*        inout_frame_ptr);

            /* The pass that is open, for the builder to add what the calls do; nullptr if there is none. */
            OpenPass* get_open_pass_ptr();

            bool has_open_pass() const;

            /* True if a pass is open and renders to the resource of the view. */
            bool is_rendered_to(const ResourceView& in_view) const;

            /* Opens a pass with what is rendered to and where, unless one is open. */
            void open_if_needed(const AttachmentSet& in_attachments,
                                const uint64_t&      in_call_index,
                                const uint64_t&      in_timestamp_qpc,
                                const Viewport&      in_viewport);

        private:
            /* True for the constants that describe the camera, which is the same for all the draws of a pass. */
            static bool is_camera_semantic(const ConstantSemantic& in_semantic);

            bool     m_has_open_pass;
            OpenPass m_open_pass;
        };
    }
}

#endif /* APIDUMPANALYSIS_PASS_TRACKER_H */

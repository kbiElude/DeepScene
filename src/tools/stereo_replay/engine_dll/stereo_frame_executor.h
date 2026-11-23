/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_STEREO_FRAME_EXECUTOR_H
#define STEREO_REPLAY_ENGINEDLL_STEREO_FRAME_EXECUTOR_H

#include "i_frame_executor.h"
#include "i_frame_image_sink.h"
#include "types.h"
#include "eye_matrix_binder.h"
#include "frame_buffer.h"
#include "hurried_frame_selector.h"
#include "frame_rewriter.h"
#include "slice_skipper.h"
#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        struct CompletedFrame;

        namespace Frontends
        {
            class AnalysisSession;
        }
    }
}

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Plays frames per eye as the recipe says: known frames are buffered and executed once or twice (matrix calls replaced,
             * screen-space draws shifted), others as read. The back buffer goes to the image sink and is kept per eye if frames carry it over.
             */
            class StereoFrameExecutor : public ::Replay::ReplayEngineDLL::IFrameExecutor
            {
            public:
                StereoFrameExecutor();
                ~StereoFrameExecutor();

                bool  execute_frame      (::Replay::ReplayEngineDLL::IFrameHost*  in_host_ptr,
                                          bool*                                   out_has_workload_ended_ptr) override;
                float get_eye_separation () const;
                bool  is_per_eye_texture (const Capture::APIDumpLoader::APIGroup& in_group,
                                          const uint64_t&                         in_address) const;
                void  on_frame_presented () override;
                void  on_rewind          () override;
                void  set_eye_separation (const float&                            in_eye_separation);
                void  set_frame_execution(const FrameExecutionOptions&            in_options);

                /* Hurried frames are not read back for the image sink. Not owned; may be null. */
                void set_hurry     (const HurriedFrameSelector* in_hurry_ptr);
                void set_image_sink(IFrameImageSink*  in_sink_ptr);

                /* Dead frames are played as read, not rewritten for the eyes. Not owned; may be null. */
                void set_slice_skipper(const SliceSkipper*             in_slice_skipper_ptr);
                void set_recipe       (const Analysis::Recipe::Recipe& in_recipe);

            private:
                struct EyeSnapshot
                {
                    bool                 has_pixels = false;
                    std::vector<uint8_t> pixel_u8_vec;
                };

                /* Back buffer left by each eye's last execution, for frames that carry it over. Replay thread only. */
                struct EyeHistory
                {
                    std::vector<uint8_t>       base_u8_vec; /* back buffer before the frame's first execution; start for an eye without history */
                    std::array<EyeSnapshot, 2> snapshot_array; /* left, right */
                };

                bool begin_frame_execution (const bool&     in_uses_history,
                                            const uint32_t& in_n_execution);
                bool capture_execution     (const uint32_t& in_n_frame,
                                            const uint32_t& in_n_execution,
                                            const bool&     in_uses_history);
                bool execute_buffered_frame(const uint32_t& in_n_frame,
                                            bool*           out_has_workload_ended_ptr);
                bool execute_direct_frame  (const uint32_t& in_n_frame,
                                            bool*           out_has_workload_ended_ptr);

                /* Shifts the draw's vertices for the eye if it is in screen space and the recipe says how; else executes it as is. */
                bool execute_draw_call     (const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                            const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                            const bool&                                    in_is_affected,
                                            const bool&                                    in_is_presented,
                                            const bool&                                    in_is_pushed,
                                            const uint64_t&                                in_cover_key,
                                            const Analysis::Recipe::Eye&                   in_eye,
                                            const uint32_t&                                in_pass_structure_id);
                bool execute_rewritten_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                            const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                            const size_t&                                  in_n_call,
                                            const uint32_t&                                in_n_execution);

                /* Executes the call that begins the camera of the affected draws, loading it with the eye shift (see ViewAnchorShift). */
                bool execute_view_anchor_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                              const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                              const Analysis::Recipe::Eye&                   in_eye);
                /* Sets the pass viewport moved right by in_shift_pixels (0 restores it), built from in_call. A draw that reaches the sides
                 * is widened (see PushShiftPlanner::make_viewport()).
                 */
                bool execute_viewport_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           const Analysis::APIDumpAnalysis::Viewport&   in_viewport,
                                           const float&                                 in_shift_pixels,
                                           const bool&                                  in_is_side_reaching);

                /* Finds the cut call (see FrameExecutionOptions::truncates_after_draw): last call of the structure's draws in the pass. */
                void find_truncation_call(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                          const uint64_t&                                  in_truncate_pass_key,
                                          const uint64_t&                                  in_truncate_draw_key);

                /* Finds the calls that begin the camera of affected draws (m_view_anchor_call_index_set). Returns false and sets the reason
                 * if an affected draw has no such call or one call serves both affected and unaffected draws (the frame is not played).
                 */
                bool find_view_anchor_calls(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                            std::string*                                     out_error_ptr);
                /* The viewport that a pushed draw is moved from: the one the device has now (the program's, or its window's if it set none), asked of the
                 * executors; in_pass_viewport if no executor can tell. The viewport of the analysis is not used for a program that sets none, because it
                 * takes the size of its window from the call that created it, which is not the size the window of the replay has.
                 */
                Analysis::APIDumpAnalysis::Viewport find_base_viewport(const Analysis::APIDumpAnalysis::Viewport& in_pass_viewport) const;
                /* Finds the played draws the recipe pushes (see Recipe::pushed), keyed by first call. */
                void find_pushed_draw_calls(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame);
                /* Finds the calls before a left-out draw's own call (glBegin, vertices) that are left out with it; a draw missing only its glEnd would still show. */
                void find_skipped_call_ranges(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame);
                bool get_eye_history_need    (const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                              bool*                                            out_needs_history_ptr,
                                              std::string*                                     out_error_ptr) const;

                /* The viewport of the pass of a draw, as the analysis has it. */
                const Analysis::APIDumpAnalysis::Viewport* get_pass_viewport_ptr(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                                                                 const Analysis::APIDumpAnalysis::DrawInstance&   in_draw) const;

                /* Returns the sideways viewport shift in pixels (see PushShiftPlanner) for a viewport that is in_viewport_width wide; 0 if the draw is not pushed. */
                float get_push_shift_pixels(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                            const Analysis::APIDumpAnalysis::DrawInstance&   in_draw,
                                            const Analysis::Recipe::Eye&                     in_eye,
                                            const float&                                     in_viewport_width) const;

                /* RecipeApplier::decide_draw() adjusted for the variant (draws it leaves out are skipped). A draw sharing its camera with an
                 * affected draw cannot stay unshifted because of its cover key (see find_view_anchor_calls()).
                 */
                Analysis::Recipe::DrawDecision decide_draw_here(const uint64_t&                                in_pass_key,
                                                                const Analysis::APIDumpAnalysis::DrawInstance& in_draw) const;

                bool is_call_skipped(const uint64_t& in_call_index) const;

                /* See FrameVariant. */
                bool is_skipped_by_variant(const uint64_t& in_pass_key,
                                           const uint64_t& in_draw_key) const;

                /* Finds left-out calls, the cut and camera-beginning calls for the variant (null: as the recipe says). False in the cases
                 * of find_view_anchor_calls().
                 */
                bool prepare_variant  (const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                       const FrameVariant*                              in_variant_ptr,
                                       std::string*                                     out_error_ptr);
                bool read_back_buffer (std::vector<uint8_t>*                            out_pixel_u8_vec_ptr,
                                       FrameImage*                                      out_image_ptr);
                bool read_next_call   (const Capture::APIDumpLoader::DumpedAPICall**    out_call_ptr_ptr);
                void set_executors_eye(const Analysis::Recipe::Eye&                     in_eye);

                /* See IEyeExecutor::set_n_executions. */
                void set_executors_n_executions(const uint32_t& in_n_executions);

                bool write_back_buffer(const std::vector<uint8_t>& in_pixel_u8_vec);

                EyeHistory                                                             m_eye_history;
                std::atomic<float>                                                     m_eye_separation; /* any thread; read once per frame */
                FrameBuffer                                                            m_frame_buffer; /* current frame (replay thread only) */
                float                                                                  m_frame_convergence_distance; /* convergence distance of the current frame; 0: none */
                FrameExecutionOptions                                                  m_frame_execution;
                float                                                                  m_frame_eye_separation; /* separation of the current frame */
                FrameExecution                                                         m_frame_mode; /* current frame; with VARIANTS: VARIANTS for the last frame, TWICE before it */
                ::Replay::ReplayEngineDLL::IFrameHost*                                 m_host_ptr; /* current frame */
                const HurriedFrameSelector*                                            m_hurry_ptr;
                IFrameImageSink*                                                       m_image_sink_ptr;
                const SliceSkipper*                                                    m_slice_skipper_ptr;
                std::unique_ptr<EyeMatrixBinder>                                       m_matrix_binder_ptr;
                std::map<uint64_t, uint32_t>                                           m_pushed_draw_by_first_call_map; /* current frame: first call of a pushed draw -> draw index */
                std::unique_ptr<Analysis::Recipe::RecipeApplier>                       m_recipe_applier_ptr;
                std::unique_ptr<FrameRewriter>                                         m_rewriter_ptr;
                std::map<uint64_t, Analysis::APIDumpAnalysis::Viewport>                m_saved_viewport_by_draw_call_map; /* current frame: own call of a pushed draw -> the viewport put back after it */
                std::unique_ptr<Analysis::APIDumpAnalysis::Frontends::AnalysisSession> m_session_ptr;
                uint64_t                                                               m_truncate_after_call; /* current frame: later calls are left out; UINT64_MAX: no cut */
                std::vector<std::pair<uint64_t, uint64_t> >                            m_skipped_call_range_vec; /* current frame: (first, last) call ranges, in call order */
                std::set<uint64_t>                                                     m_unflattened_draw_call_set; /* current frame: calls of cover-key-pushed draws that keep the rules (shared camera with affected draws) */
                std::set<uint64_t>                                                     m_view_anchor_call_index_set; /* current frame: calls that begin the affected draws' camera */
                const FrameVariant*                                                    m_variant_ptr; /* current execution in VARIANTS mode; null otherwise */
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_STEREO_FRAME_EXECUTOR_H */

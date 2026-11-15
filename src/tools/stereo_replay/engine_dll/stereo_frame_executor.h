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
#include "frame_hurry.h"
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
            /* Plays the frames for the eyes of a recipe. A frame whose shape the recipe knows is read up to its end and then executed
             * once or twice. The calls that set a matrix are replaced as the recipe says, and draws that are in screen space (the space
             * of the picture) are shifted for the eye. Other frames are played as they are read. After each execution the back buffer is
             * handed to the image sink. For frames that carry the back buffer over from the previous frame, the back buffer is also
             * kept separately for each eye.
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

                /* Sets the hurry. Frames that it says are hurried through are not read back for the image sink. Not owned; may be null. */
                void set_hurry     (const FrameHurry* in_hurry_ptr);
                void set_image_sink(IFrameImageSink*  in_sink_ptr);

                /* Frames that the skipper says are dead are played as they are, not rewritten for the eyes (see SliceSkipper). Not owned;
                 * may be null.
                 */
                void set_slice_skipper(const SliceSkipper*             in_slice_skipper_ptr);
                void set_recipe       (const Analysis::Recipe::Recipe& in_recipe);

            private:
                struct EyeSnapshot
                {
                    bool                 has_pixels = false;
                    std::vector<uint8_t> pixel_u8_vec;
                };

                /* The back buffer left by the last execution of each eye, for frames that carry the back buffer over. An execution
                 * starts from the buffer of its own eye. Replay thread only.
                 */
                struct EyeHistory
                {
                    std::vector<uint8_t>       base_u8_vec; /* the back buffer before the first execution of the frame; an eye without history starts from it */
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

                /* Executes a draw. If the draw is in screen space and the recipe says how to shift it, its vertices are shifted for the
                 * eye (see ScreenParallax). Otherwise the draw is executed as it is. in_pass_structure_id is the structure of the pass that
                 * the draw is in.
                 */
                bool execute_draw_call     (const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                            const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                            const bool&                                    in_is_affected,
                                            const bool&                                    in_is_presented,
                                            const uint64_t&                                in_cover_key,
                                            const Analysis::Recipe::Eye&                   in_eye,
                                            const uint32_t&                                in_pass_structure_id);
                bool execute_rewritten_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                            const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                            const size_t&                                  in_n_call,
                                            const uint32_t&                                in_n_execution);

                /* Executes the call that begins the camera of the draws that the recipe affects (see ViewAnchorShift). The call loads the
                 * camera together with the shift of the eye.
                 */
                bool execute_view_anchor_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                              const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                              const Analysis::Recipe::Eye&                   in_eye);
                /* Sets the OpenGL viewport to the viewport of the pass, moved right by in_shift_pixels (0 restores the original). The new
                 * call is built from in_call. A pushed draw runs between the shifting call and the restoring call. A draw that reaches the
                 * sides is widened (see PushShiftPlanner::make_viewport()).
                 */
                bool execute_viewport_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           const Analysis::APIDumpAnalysis::Viewport&   in_viewport,
                                           const float&                                 in_shift_pixels,
                                           const bool&                                  in_is_side_reaching);

                /* Finds the call after which the frame is cut (see FrameExecutionOptions::truncates_after_draw): the last call of the draws
                 * with the given structure in the given pass.
                 */
                void find_truncation_call(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                          const uint64_t&                                  in_truncate_pass_key,
                                          const uint64_t&                                  in_truncate_draw_key);

                /* Finds the calls that begin the camera of the draws of the frame that the recipe affects (see m_view_anchor_call_index_set).
                 * Returns false, and sets the reason, if an affected draw has no camera that a call begins, or if one call begins the camera
                 * of both affected and unaffected draws. The eye of such a draw cannot be built, so the frame is not played, instead of
                 * being played with a wrong picture.
                 */
                bool find_view_anchor_calls(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                            std::string*                                     out_error_ptr);
                /* Finds the draws of the frame that the recipe pushes (see Recipe::pushed) and that are played. Each draw is recorded under
                 * its first call, as its index in the draws of the frame.
                 */
                void find_pushed_draw_calls(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame);
                /* Finds the calls that belong to a draw that the recipe leaves out and come before the draw's own call (the glBegin and the
                 * vertices of a block that a glEnd turns into a draw). They are left out with the draw: a primitive is drawn as its vertices
                 * are given, so a draw that lost only its glEnd would still be visible.
                 */
                void find_skipped_call_ranges(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame);
                bool get_eye_history_need    (const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                              bool*                                            out_needs_history_ptr,
                                              std::string*                                     out_error_ptr) const;

                /* Returns how far the viewport moves sideways for the draw, in pixels (see PushShiftPlanner), and the viewport of its pass
                 * through out_viewport_ptr_ptr. Returns 0 if the recipe does not push the draw.
                 */
                float get_push_shift_pixels(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                            const Analysis::APIDumpAnalysis::DrawInstance&   in_draw,
                                            const Analysis::Recipe::Eye&                     in_eye,
                                            const Analysis::APIDumpAnalysis::Viewport**      out_viewport_ptr_ptr) const;

                /* What is done to the draw, as RecipeApplier::decide_draw() says, and as the variant the frame is being executed as (see
                 * FrameVariant) changes it: a draw that the variant leaves out is skipped. The replay applies the shift of an eye where a
                 * camera begins, so a draw that shares its camera with an affected draw cannot be left alone because of its cover key (see
                 * find_view_anchor_calls()).
                 */
                Analysis::Recipe::DrawDecision decide_draw_here(const uint64_t&                                in_pass_key,
                                                                const Analysis::APIDumpAnalysis::DrawInstance& in_draw) const;

                bool is_call_skipped(const uint64_t& in_call_index) const;

                /* Whether the variant leaves the draw out (see FrameVariant). */
                bool is_skipped_by_variant(const uint64_t& in_pass_key,
                                           const uint64_t& in_draw_key) const;

                /* Prepares the frame for execution as the variant (null: as the recipe says). It finds the calls that are left out, where
                 * the frame is cut, and the calls that begin the camera of the draws that are played. Returns false, and sets the reason,
                 * in the cases that find_view_anchor_calls() describes.
                 */
                bool prepare_variant  (const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                       const FrameVariant*                              in_variant_ptr,
                                       std::string*                                     out_error_ptr);
                bool read_back_buffer (std::vector<uint8_t>*                            out_pixel_u8_vec_ptr,
                                       FrameImage*                                      out_image_ptr);
                bool read_next_call   (const Capture::APIDumpLoader::DumpedAPICall**    out_call_ptr_ptr);
                void set_executors_eye(const Analysis::Recipe::Eye&                     in_eye);

                /* Tells the executors that keep a copy of an object per eye how many times the frame is executed (see
                 * IEyeExecutor::set_n_executions).
                 */
                void set_executors_n_executions(const uint32_t& in_n_executions);

                bool write_back_buffer(const std::vector<uint8_t>& in_pixel_u8_vec);

                EyeHistory                                                             m_eye_history;
                std::atomic<float>                                                     m_eye_separation; /* set by any thread; read once per frame */
                FrameBuffer                                                            m_frame_buffer; /* the frame being played (replay thread only) */
                FrameExecutionOptions                                                  m_frame_execution;
                float                                                                  m_frame_eye_separation; /* the separation that the frame being played uses */
                FrameExecution                                                         m_frame_mode; /* of the frame being played; with the VARIANTS option, VARIANTS for the last frame and TWICE for the frames before it */
                ::Replay::ReplayEngineDLL::IFrameHost*                                 m_host_ptr; /* of the frame being played */
                const FrameHurry*                                                      m_hurry_ptr;
                IFrameImageSink*                                                       m_image_sink_ptr;
                const SliceSkipper*                                                    m_slice_skipper_ptr;
                std::unique_ptr<EyeMatrixBinder>                                       m_matrix_binder_ptr;
                std::map<uint64_t, uint32_t>                                           m_pushed_draw_by_first_call_map; /* of the frame being played: first call of a pushed draw -> index of the draw */
                std::unique_ptr<Analysis::Recipe::RecipeApplier>                       m_recipe_applier_ptr;
                std::unique_ptr<FrameRewriter>                                         m_rewriter_ptr;
                std::unique_ptr<Analysis::APIDumpAnalysis::Frontends::AnalysisSession> m_session_ptr;
                uint64_t                                                               m_truncate_after_call; /* of the frame being played: the calls after this one are left out; UINT64_MAX: no cut */
                std::vector<std::pair<uint64_t, uint64_t> >                            m_skipped_call_range_vec; /* of the frame being played: ranges of calls (first, last), in call order */
                std::set<uint64_t>                                                     m_unflattened_draw_call_set; /* of the frame being played: the calls of draws that are pushed by cover key but keep the rules, because they share their camera with draws that the rules affect */
                std::set<uint64_t>                                                     m_view_anchor_call_index_set; /* of the frame being played: the calls that begin the camera of the affected draws */
                const FrameVariant*                                                    m_variant_ptr; /* of the current execution in the VARIANTS mode; null otherwise */
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_STEREO_FRAME_EXECUTOR_H */

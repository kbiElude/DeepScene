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
            /* Plays the frames for the eyes of a recipe. A frame of a shape the recipe knows is read up to its end, and then executed once or
             * twice, with the calls which set a matrix replaced as the recipe says, and the draws moved for the eye if they are in the space of
             * the picture. The other frames are played as they are read. After each execution the back buffer is handed to the image sink, and
             * is kept apart for each eye, for the frames that carry it over.
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

                /* The frames which the hurry says are hurried through are not read back for the sink. Not owned; may be none. */
                void set_hurry     (const FrameHurry* in_hurry_ptr);
                void set_image_sink(IFrameImageSink*  in_sink_ptr);

                /* The frames which the skipper says are dead are played as they are (see SliceSkipper). Not owned; may be none. */
                void set_slice_skipper(const SliceSkipper*             in_slice_skipper_ptr);
                void set_recipe       (const Analysis::Recipe::Recipe& in_recipe);

            private:
                struct EyeSnapshot
                {
                    bool                 has_pixels = false;
                    std::vector<uint8_t> pixel_u8_vec;
                };

                /* The back buffer left by the last execution of each eye, for frames that carry it over: an execution starts from the
                 * one of its own eye. Replay thread only.
                 */
                struct EyeHistory
                {
                    std::vector<uint8_t>       base_u8_vec; /* before the first execution of the frame, for an eye which has no history yet */
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

                /* A draw: with its vertices moved for the eye if they are in the space of the picture and the recipe says how (see ScreenParallax),
                 * as it is otherwise. The pass is the structure of the pass which the draw is in.
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

                /* The call that begins the camera of the draws that the recipe affects (see ViewAnchorShift), played with the shift of the eye in place of what it loads. */
                bool execute_view_anchor_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                              const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                              const Analysis::Recipe::Eye&                   in_eye);
                /* Sets the viewport (OpenGL) to the one of the pass moved to the right by the pixels, with the call of a draw as the call to make a copy of; with 0 it is the viewport as it was. A draw that the
                 * recipe pushes (see Recipe::pushed) is made between the two, so that its picture is moved sideways whatever its projection is.
                 */
                bool execute_viewport_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                           const Analysis::APIDumpAnalysis::Viewport&     in_viewport,
                                           const float&                                   in_shift_pixels);

                /* The calls of the draws that the recipe leaves out and which come before the call of the draw (the glBegin and the vertices of a block that a
                 * glEnd makes a draw of) are left out with it: a primitive is drawn as its vertices are given, and a draw that only loses its glEnd is still seen.
                 */
                /* The call after which the frame is cut (see FrameExecutionOptions::truncates_after_draw): the last call of the draws of the structure in the pass. */
                void find_truncation_call    (const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                              const uint64_t&                                  in_truncate_pass_key,
                                              const uint64_t&                                  in_truncate_draw_key);

                /* The calls that begin the camera of the draws of the frame that the recipe affects (see m_view_anchor_call_index_set). False, and the reason, if a draw that is
                 * affected has no camera that a call begins, or one call begins the camera of draws of which some are affected and some are not: the eye of such a draw
                 * cannot be made, and it is not played for one that is not what it is.
                 */
                bool find_view_anchor_calls(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                            std::string*                                     out_error_ptr);
                /* The draws of the frame that the recipe pushes (see Recipe::pushed) and are played, by the first call of each: its place in the draws of the frame. */
                void find_pushed_draw_calls  (const Analysis::APIDumpAnalysis::CompletedFrame& in_frame);
                void find_skipped_call_ranges(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame);
                bool get_eye_history_need    (const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                              bool*                                            out_needs_history_ptr,
                                              std::string*                                     out_error_ptr) const;

                /* How far the viewport is moved sideways for the draw, in pixels (see PushShiftPlanner), and the viewport of its pass; 0 if the recipe does not push the draw. */
                float get_push_shift_pixels  (const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                              const Analysis::APIDumpAnalysis::DrawInstance&   in_draw,
                                              const Analysis::Recipe::Eye&                     in_eye,
                                              const Analysis::APIDumpAnalysis::Viewport**      out_viewport_ptr_ptr) const;

                /* Whether the rules apply to the draw: the recipe says so, and the draw is not one that is named by its size as pushed (those are flat: see DrawSelector::cover_key_vec). */
                bool is_draw_affected_here   (const uint64_t&                                  in_pass_key,
                                              const Analysis::APIDumpAnalysis::DrawInstance&   in_draw) const;

                /* Whether the recipe pushes the draw: by the draw in its pass, its key, the key of its pass, or the key of the draw with its size. */
                bool is_draw_pushed_here     (const uint64_t&                                  in_pass_key,
                                              const Analysis::APIDumpAnalysis::DrawInstance&   in_draw) const;

                /* Whether a draw is left out: by the recipe, or by the variant that the frame is being executed as (see FrameVariant). */
                bool is_draw_skipped_here    (const uint64_t&                                  in_pass_key,
                                              const uint64_t&                                  in_draw_key) const;
                bool is_call_skipped         (const uint64_t&                                  in_call_index) const;

                /* Makes the frame ready to be executed as the variant (none: as the recipe says): the calls that are left out, where the frame is cut, and the calls that
                 * begin the camera of the draws that are played. False, and the reason, as find_view_anchor_calls() says.
                 */
                bool prepare_variant         (const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                              const FrameVariant*                              in_variant_ptr,
                                              std::string*                                     out_error_ptr);
                bool read_back_buffer        (std::vector<uint8_t>*                            out_pixel_u8_vec_ptr,
                                              FrameImage*                                      out_image_ptr);
                bool read_next_call          (const Capture::APIDumpLoader::DumpedAPICall**    out_call_ptr_ptr);
                void set_executors_eye       (const Analysis::Recipe::Eye&                     in_eye);

                /* How many times the frame is executed, for the executors that keep a copy of an object for each eye (see IEyeExecutor::set_n_executions). */
                void set_executors_n_executions(const uint32_t& in_n_executions);

                bool write_back_buffer(const std::vector<uint8_t>& in_pixel_u8_vec);

                EyeHistory                                                             m_eye_history;
                std::atomic<float>                                                     m_eye_separation; /* set by any thread; read once for each frame */
                FrameBuffer                                                            m_frame_buffer; /* the frame being played, in the replay thread */
                FrameExecutionOptions                                                  m_frame_execution;
                float                                                                  m_frame_eye_separation; /* the one the frame being played is made with */
                FrameExecution                                                         m_frame_mode; /* of the frame being played: VARIANTS only for the last frame; TWICE for those before it */
                ::Replay::ReplayEngineDLL::IFrameHost*                                 m_host_ptr; /* of the frame being played */
                const FrameHurry*                                                      m_hurry_ptr;
                IFrameImageSink*                                                       m_image_sink_ptr;
                const SliceSkipper*                                                    m_slice_skipper_ptr;
                std::unique_ptr<EyeMatrixBinder>                                       m_matrix_binder_ptr;
                std::map<uint64_t, uint32_t>                                           m_pushed_draw_by_first_call_map; /* of the frame being played: the first call of a draw that is pushed -> the draw */
                std::unique_ptr<Analysis::Recipe::RecipeApplier>                       m_recipe_applier_ptr;
                std::unique_ptr<FrameRewriter>                                         m_rewriter_ptr;
                std::unique_ptr<Analysis::APIDumpAnalysis::Frontends::AnalysisSession> m_session_ptr;
                uint64_t                                                               m_truncate_after_call; /* of the frame being played: the calls after it are left out; UINT64_MAX: none */
                std::vector<std::pair<uint64_t, uint64_t> >                            m_skipped_call_range_vec; /* of the frame being played: from the first call to the last, in order */
                std::set<uint64_t>                                                     m_unflattened_draw_call_set; /* of the frame being played: the calls of the draws that are named by their size as pushed, and keep the rules, as the camera that they are made with is the one of draws that the rules affect */
                std::set<uint64_t>                                                     m_view_anchor_call_index_set; /* of the frame being played: the calls that begin the camera of the draws that are affected */
                const FrameVariant*                                                    m_variant_ptr; /* of the execution that is being made, in the mode VARIANTS; none otherwise */
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_STEREO_FRAME_EXECUTOR_H */

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
#include <memory>
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
                bool execute_draw_call     (const Capture::APIDumpLoader::DumpedAPICall&     in_call,
                                            const ::Replay::ReplayEngineDLL::IChunkSource&   in_chunk_source,
                                            const bool&                                      in_is_affected,
                                            const bool&                                      in_is_presented,
                                            const uint64_t&                                  in_cover_key,
                                            const Analysis::Recipe::Eye&                     in_eye,
                                            const uint32_t&                                  in_pass_structure_id);
                bool execute_rewritten_call(const Capture::APIDumpLoader::DumpedAPICall&     in_call,
                                            const ::Replay::ReplayEngineDLL::IChunkSource&   in_chunk_source,
                                            const size_t&                                    in_n_call,
                                            const uint32_t&                                  in_n_execution);
                bool get_eye_history_need  (const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                            bool*                                            out_needs_history_ptr,
                                            std::string*                                     out_error_ptr) const;
                bool read_back_buffer      (std::vector<uint8_t>*                            out_pixel_u8_vec_ptr,
                                            FrameImage*                                      out_image_ptr);
                bool read_next_call        (const Capture::APIDumpLoader::DumpedAPICall**    out_call_ptr_ptr);
                void set_executors_eye     (const Analysis::Recipe::Eye&                     in_eye);
                bool write_back_buffer     (const std::vector<uint8_t>&                      in_pixel_u8_vec);

                EyeHistory                                                             m_eye_history;
                std::atomic<float>                                                     m_eye_separation; /* set by any thread; read once for each frame */
                FrameBuffer                                                            m_frame_buffer; /* the frame being played, in the replay thread */
                FrameExecutionOptions                                                  m_frame_execution;
                float                                                                  m_frame_eye_separation; /* the one the frame being played is made with */
                ::Replay::ReplayEngineDLL::IFrameHost*                                 m_host_ptr; /* of the frame being played */
                const FrameHurry*                                                      m_hurry_ptr;
                IFrameImageSink*                                                       m_image_sink_ptr;
                const SliceSkipper*                                                    m_slice_skipper_ptr;
                std::unique_ptr<EyeMatrixBinder>                                       m_matrix_binder_ptr;
                std::unique_ptr<Analysis::Recipe::RecipeApplier>                       m_recipe_applier_ptr;
                std::unique_ptr<FrameRewriter>                                         m_rewriter_ptr;
                std::unique_ptr<Analysis::APIDumpAnalysis::Frontends::AnalysisSession> m_session_ptr;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_STEREO_FRAME_EXECUTOR_H */

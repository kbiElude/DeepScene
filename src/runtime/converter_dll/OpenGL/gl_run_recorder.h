/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_RUN_RECORDER_H
#define STEREO_CONVERTERDLL_GL_RUN_RECORDER_H

#include "types.h"
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Keeps what the right eye does for a run of the application's calls (the blocks that are drawn, and the state calls between them), so that it is done once
         * for the whole run instead of once for each block. The left eye is live: the application's calls reach the driver as they are made, and the blocks are drawn
         * for the left eye when they end. The right eye needs the framebuffer of the right eye bound, and a bind is costly because the driver pays for it at the
         * next draw. Only what the converter can give back with the driver's functions is kept (see GLRunCallClassifier).
         */
        class GLRunRecorder
        {
        public:
            GLRunRecorder();

            /* Keeps the colors, normals and texture coordinates that the application sets outside a block (the current values that the next block starts with). */
            void add_attribute(const RecordedCall& in_call);

            /* Keeps a block to be drawn for the right eye; the calls of the block are copied. */
            void add_draw(const RunDraw&      in_draw,
                          const RecordedCall* in_calls_ptr);

            void add_state(const RunStateCall& in_call);

            /* Forgets the items. The memory is kept for the next run. */
            void clear();

            uint32_t get_n_draws() const;

            bool is_empty() const;

            /* Makes the items for the right eye, in the order they were kept, with the driver's functions. The framebuffer of the right eye is bound, and the state
             * is that of the start of the run. A draw loads its own matrices, so the matrix state when the run was played is not what the application had.
             */
            void play() const;

        private:
            static void play_state(const RunStateCall& in_call);

            std::vector<RecordedCall> m_call_vec;
            std::vector<RunDraw>      m_draw_vec;
            std::vector<RunItem>      m_item_vec;
            std::vector<RunStateCall> m_state_vec;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_RUN_RECORDER_H */

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_BLOCK_RECORDER_H
#define STEREO_CONVERTERDLL_GL_BLOCK_RECORDER_H

#include "common/api_function_argument.h"
#include "types.h"
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Keeps the calls of one glBegin..glEnd block of the application (the vertices, and the colors, normals and texture coordinates between them), and
         * makes them again, as the application made them, for each eye. The application makes such a block natively at a cost that the driver's display
         * list compile, which the renderer otherwise uses, does not come near: the calls are cheap, and the block is made in the same way for each eye.
         * Only what the application can be given back with the driver's own functions is recorded: find_layout() says which calls those are.
         */
        class GLBlockRecorder
        {
        public:
            GLBlockRecorder();

            void add(const RecordedCallKind&          in_kind,
                     const std::array<float, 4>&      in_value_fp32vec4);

            /* Forgets the calls and starts a block of the mode. The memory of the calls is kept for the next block. */
            void begin(const uint32_t& in_mode);

            const std::vector<RecordedCall>& get_calls() const;

            uint32_t get_mode() const;

            /* Makes the block with the driver's functions: glBegin, the calls, glEnd. */
            void play() const;

            /* Makes the calls only, with the driver's functions (the block is being given to a display list that has had its glBegin). */
            void play_calls() const;

            /* Says what a call sets, if it is a call that is recorded. */
            static bool find_layout(const APIInterceptor::APIFunction& in_function,
                                    RecordedCallLayout*                out_layout_ptr);

            /* Whether the kind of call gives a vertex (the others set an attribute of the vertices that follow). */
            static bool is_vertex(const RecordedCallKind& in_kind);

            /* Makes a block that is kept elsewhere: glBegin, the calls, glEnd. */
            static void play_block(const uint32_t&     in_mode,
                                   const RecordedCall* in_calls_ptr,
                                   const size_t&       in_n_calls);

            /* Makes calls that are kept elsewhere. */
            static void play_span(const RecordedCall* in_calls_ptr,
                                  const size_t&       in_n_calls);

        private:
            std::vector<RecordedCall> m_call_vec;
            uint32_t                  m_mode;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_BLOCK_RECORDER_H */

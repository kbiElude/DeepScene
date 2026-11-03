/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_INTERNAL_TYPES_H
#define STEREO_REPLAY_ENGINEDLL_INTERNAL_TYPES_H

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* When the calls of a frame that is executed more than once are executed. */
            enum class ExecutionClass
            {
                AFTER_ALL,      /* once, after the last execution: what destroys objects, and the present */
                EVERY_TIME,     /* in every execution: state, draws, uploads                              */
                FIRST_TIME_ONLY /* once, in the first execution: what creates objects                     */
            };

            /* What a handle or an ID of the dumped process stands for. An object is told apart by the kind and the value it had
             * in the dump (see ObjectRemapper). NONE stands for a value which is not one of those.
             */
            enum class ObjectKind
            {
                ATTRIB_LOCATION,
                BUFFER,
                D3D7_OBJECT,
                D3D8_OBJECT,
                D3D8_PIXEL_SHADER,
                D3D8_STATE_BLOCK,
                D3D8_VERTEX_SHADER,
                DISPLAY_LIST,
                FRAG_DATA_LOCATION,
                FRAMEBUFFER,
                HDC,
                HGLRC,
                HWND,
                NONE,
                PROGRAM,
                PROGRAM_PIPELINE,
                PROGRAM_RESOURCE_INDEX,
                QUERY,
                RENDERBUFFER,
                SAMPLER,
                SHADER,
                SUBROUTINE_INDEX,
                SUBROUTINE_UNIFORM_LOCATION,
                SYNC,
                TEXTURE,
                TRANSFORM_FEEDBACK,
                UNIFORM_BLOCK_INDEX,
                UNIFORM_LOCATION,
                VERTEX_ARRAY
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_INTERNAL_TYPES_H */

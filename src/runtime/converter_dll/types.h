/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The enums of the converter.
 */
#ifndef CONVERTERDLL_TYPES_H
#define CONVERTERDLL_TYPES_H

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Where the vertices of a draw are stored. */
        enum class DrawKind
        {
            STRIDED,           /* in arrays of the application, which a structure points to
 */
            VERTEX_BUFFER,     /* in a vertex buffer: they are read into the memory of the renderer, and from then on they are in memory
 */
            VERTICES_IN_MEMORY /* in one block of the memory of the application
 */
        };
    }
}

#endif /* CONVERTERDLL_TYPES_H */

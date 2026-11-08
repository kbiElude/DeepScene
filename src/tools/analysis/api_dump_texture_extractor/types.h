/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the extractor of the textures.
 */
#ifndef APIDUMPTEXTUREEXTRACTOR_TYPES_H
#define APIDUMPTEXTUREEXTRACTOR_TYPES_H

namespace Analysis
{
    namespace APIDumpTextureExtractor
    {
        /* What the tool exits with. */
        enum class ExitCode
        {
            BAD_ARGUMENTS           = 1,
            CANNOT_READ_WORKLOAD    = 2,
            CANNOT_WRITE_TEXTURES   = 3,
            SUCCESS                 = 0,
            TEXTURES_ARE_INCOMPLETE = 4  /* the workload was read, but what it holds of the textures could not all be kept */
        };
    }
}

#endif /* APIDUMPTEXTUREEXTRACTOR_TYPES_H */

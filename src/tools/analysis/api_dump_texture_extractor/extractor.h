/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Extractor tool logic (defined in main.cpp).
 */
#ifndef APIDUMPTEXTUREEXTRACTOR_EXTRACTOR_H
#define APIDUMPTEXTUREEXTRACTOR_EXTRACTOR_H

#include "analysis/api_dump_textures/types.h"
#include "types.h"

namespace Analysis
{
    namespace APIDumpTextureExtractor
    {
        void print_stats(const Analysis::APIDumpTextures::TextureTimeline& in_timeline);
        void print_usage();

        /* Entry point of the tool (called by the global main). */
        int run(int   in_argc,
                char* in_argv[]);
    }
}

#endif /* APIDUMPTEXTUREEXTRACTOR_EXTRACTOR_H */

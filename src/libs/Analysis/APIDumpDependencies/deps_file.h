/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads and writes .deps files (see deps_format.h). The same analysis always gives the same bytes.
 */
#ifndef APIDUMPDEPENDENCIES_DEPS_FILE_H
#define APIDUMPDEPENDENCIES_DEPS_FILE_H

#include "Analysis/APIDumpDependencies/types.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        class DepsFile
        {
        public:
            /* The error is set when the function returns false. */
            static bool read(const std::string&  in_filename,
                             DependencyAnalysis* out_analysis_ptr,
                             std::string*        out_error_ptr);

            static bool                 read_bytes (const std::vector<uint8_t>& in_byte_vec,
                                                    DependencyAnalysis*         out_analysis_ptr,
                                                    std::string*                out_error_ptr);
            static bool                 write      (const std::string&          in_filename,
                                                    const DependencyAnalysis&   in_analysis,
                                                    std::string*                out_error_ptr);
            static std::vector<uint8_t> write_bytes(const DependencyAnalysis&   in_analysis);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_DEPS_FILE_H */

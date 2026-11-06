/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Converts the parts of a dependency analysis to and from the bytes of .deps sections (see deps_format.h). Reading validates: impossible counts,
 * frames out of range or out of order, and leftover bytes make a section invalid. Sections must be read in file order, since later ones are
 * checked against earlier ones.
 */
#ifndef APIDUMPDEPENDENCIES_DEPS_SERIALIZER_H
#define APIDUMPDEPENDENCIES_DEPS_SERIALIZER_H

#include "analysis/api_dump_dependencies/types.h"
#include "utils/binary_io/byte_reader.h"
#include "utils/binary_io/byte_writer.h"
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        class DepsSerializer
        {
        public:
            static bool                 deserialize_dependencies(const std::vector<uint8_t>& in_byte_vec,
                                                                 DependencyAnalysis*         inout_analysis_ptr);
            static bool                 deserialize_frames      (const std::vector<uint8_t>& in_byte_vec,
                                                                 DependencyAnalysis*         inout_analysis_ptr);
            static bool                 deserialize_meta        (const std::vector<uint8_t>& in_byte_vec,
                                                                 DependencyAnalysis*         out_analysis_ptr);
            static bool                 deserialize_resources   (const std::vector<uint8_t>& in_byte_vec,
                                                                 DependencyAnalysis*         out_analysis_ptr);
            static std::vector<uint8_t> serialize_dependencies  (const DependencyAnalysis&   in_analysis);
            static std::vector<uint8_t> serialize_frames        (const DependencyAnalysis&   in_analysis);
            static std::vector<uint8_t> serialize_meta          (const DependencyAnalysis&   in_analysis);
            static std::vector<uint8_t> serialize_resources     (const DependencyAnalysis&   in_analysis);

        private:
            /* Reads a value of an enumeration, which goes from 0 up to the last value given. A value above it fails the reader. */
            template<typename Enum>
            static Enum read_enum(Utils::BinaryIO::ByteReader* inout_reader_ptr,
                                  const Enum&                  in_last_value);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_DEPS_SERIALIZER_H */

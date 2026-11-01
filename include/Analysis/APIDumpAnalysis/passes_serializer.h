/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Converts the parts of an analysis to and from the bytes of .passes sections (see passes_format.h). Reading
 * validates: impossible counts, dangling references and leftover bytes make a section invalid.
 *
 * Sections must be read in file order, since later ones are checked against earlier ones.
 */
#ifndef APIDUMPANALYSIS_PASSES_SERIALIZER_H
#define APIDUMPANALYSIS_PASSES_SERIALIZER_H

#include "Analysis/APIDumpAnalysis/byte_reader.h"
#include "Analysis/APIDumpAnalysis/byte_writer.h"
#include "Analysis/APIDumpAnalysis/model.h"
#include <map>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class PassesSerializer
        {
        public:
            /* The histograms that the model keeps in a member: for how deep the draws are, and for how deep their surfaces are. */
            typedef std::map<DepthHistogramKey, DepthHistogram> Model::* DepthHistogramMapMember;

            static bool                 deserialize_covering       (const std::vector<uint8_t>& in_byte_vec,
                                                                    Model*                      out_model_ptr);
            static bool                 deserialize_depths         (const std::vector<uint8_t>& in_byte_vec,
                                                                    const DepthHistogramMapMember& in_member_ptr,
                                                                    Model*                      out_model_ptr);
            static bool                 deserialize_draws          (const std::vector<uint8_t>& in_byte_vec,
                                                                    Model*                      out_model_ptr);
            static bool                 deserialize_frames         (const std::vector<uint8_t>& in_byte_vec,
                                                                    Model*                      out_model_ptr);
            static bool                 deserialize_meta           (const std::vector<uint8_t>& in_byte_vec,
                                                                    AnalysisResult*             out_analysis_ptr);
            static bool                 deserialize_passes         (const std::vector<uint8_t>& in_byte_vec,
                                                                    Model*                      out_model_ptr);
            static bool                 deserialize_representatives(const std::vector<uint8_t>& in_byte_vec,
                                                                    Model*                      out_model_ptr);
            static bool                 deserialize_resource_access(const std::vector<uint8_t>& in_byte_vec,
                                                                    Model*                      out_model_ptr);
            static bool                 deserialize_resources      (const std::vector<uint8_t>& in_byte_vec,
                                                                    Model*                      out_model_ptr);
            static bool                 deserialize_series         (const std::vector<uint8_t>& in_byte_vec,
                                                                    Model*                      out_model_ptr);
            static bool                 deserialize_shapes         (const std::vector<uint8_t>& in_byte_vec,
                                                                    Model*                      out_model_ptr);
            static std::vector<uint8_t> serialize_covering         (const Model&                in_model);
            static std::vector<uint8_t> serialize_depths           (const Model&                in_model,
                                                                    const DepthHistogramMapMember& in_member_ptr);
            static std::vector<uint8_t> serialize_draws            (const Model&                in_model);
            static std::vector<uint8_t> serialize_frames           (const Model&                in_model);
            static std::vector<uint8_t> serialize_meta             (const AnalysisResult&       in_analysis);
            static std::vector<uint8_t> serialize_passes           (const Model&                in_model);
            static std::vector<uint8_t> serialize_representatives  (const Model&                in_model);
            static std::vector<uint8_t> serialize_resource_access  (const Model&                in_model);
            static std::vector<uint8_t> serialize_resources        (const Model&                in_model);
            static std::vector<uint8_t> serialize_series           (const Model&                in_model);
            static std::vector<uint8_t> serialize_shapes           (const Model&                in_model);

        private:
            static bool is_resource_access_valid(const ResourceAccess& in_access,
                                                 const uint64_t&       in_n_frames);

            /* True for no resource, or a resource of the first n. */
            static bool is_resource_id_valid(const ResourceId& in_id,
                                             const size_t&     in_n_resources);

            static DataReference       read_data_reference       (ByteReader*                inout_reader_ptr);
            static DrawStructure       read_draw_structure       (ByteReader*                inout_reader_ptr);
            static PassInstance        read_pass_instance        (ByteReader*                inout_reader_ptr);
            static PassStructure       read_pass_structure       (ByteReader*                inout_reader_ptr);
            static ResourceDescription read_resource_description (ByteReader*                inout_reader_ptr);
            static ResourceView        read_resource_view        (ByteReader*                inout_reader_ptr);
            static TransferRecord      read_transfer_record      (ByteReader*                inout_reader_ptr);
            static Viewport            read_viewport             (ByteReader*                inout_reader_ptr);
            static void                write_data_reference      (const DataReference&       in_reference,
                                                                  ByteWriter*                inout_writer_ptr);
            static void                write_draw_structure      (const DrawStructure&       in_structure,
                                                                  ByteWriter*                inout_writer_ptr);
            static void                write_pass_instance       (const PassInstance&        in_instance,
                                                                  ByteWriter*                inout_writer_ptr);
            static void                write_pass_structure      (const PassStructure&       in_structure,
                                                                  ByteWriter*                inout_writer_ptr);
            static void                write_resource_description(const ResourceDescription& in_description,
                                                                  ByteWriter*                inout_writer_ptr);
            static void                write_resource_view       (const ResourceView&        in_view,
                                                                  ByteWriter*                inout_writer_ptr);
            static void                write_transfer_record     (const TransferRecord&      in_record,
                                                                  ByteWriter*                inout_writer_ptr);
            static void                write_viewport            (const Viewport&            in_viewport,
                                                                  ByteWriter*                inout_writer_ptr);

            /* Reads a value of an enumeration, which goes from 0 up to the last value given. A value above it fails the reader. */
            template<typename Enum>
            static Enum read_enum(ByteReader* inout_reader_ptr,
                                  const Enum& in_last_value);
        };
    }
}

#endif /* APIDUMPANALYSIS_PASSES_SERIALIZER_H */

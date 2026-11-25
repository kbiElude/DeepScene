/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Builds the model draw for a glDrawArrays or glDrawElements call whose vertices (and indices) are in application memory. The dumper copies
 * the data that such a draw reads and stores the copy in the call (see GLClientArraySnapshot). Each array and the indices become model
 * resources. A resource is created once for the same bytes, in whichever frame they appear (see GLClientArrayResources).
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_ARRAY_DRAW_DESCRIBER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_ARRAY_DRAW_DESCRIBER_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_client_array_resources.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_pipeline_state.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/types.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include <stddef.h>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLArrayDrawDescriber
            {
            public:
                /* The builder must outlive the describer. */
                explicit GLArrayDrawDescriber(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                /* Describes the draw that the call makes, using the current pipeline state. The description is written only if the result is DRAW. */
                GLArrayDrawResult describe(const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                           const Capture::APIDumpLoader::Workload&             in_workload,
                                           const bool&                                         in_is_indexed,
                                           const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                           const GLPipelineState&                              in_pipeline_state,
                                           Analysis::APIDumpAnalysis::DrawDescription*         out_description_ptr);

            private:
                /* A sample of the geometry of the draw, taken from the position array; null if none of the vertices could be read. */
                std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> make_geometry_sample(const std::vector<GLClientArray>&        in_array_vec,
                                                                                                      const Capture::APIDumpLoader::DataChunk* in_indices_chunk_ptr,
                                                                                                      const uint32_t&                          in_index_type,
                                                                                                      const int32_t&                           in_first,
                                                                                                      const int32_t&                           in_count) const;

                /* Returns the index at the given position in the index array. The type is one of the three index types that glDrawElements accepts. */
                static uint32_t read_index(const Capture::APIDumpLoader::DataChunk& in_indices_chunk,
                                           const uint32_t&                          in_index_type,
                                           const size_t&                            in_place);

                GLClientArrayResources                   m_array_resources;
                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_ARRAY_DRAW_DESCRIBER_H */

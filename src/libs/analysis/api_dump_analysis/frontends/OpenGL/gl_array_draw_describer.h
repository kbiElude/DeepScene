/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Makes a draw of the model of a call of glDrawArrays or glDrawElements whose vertices (and indices) are in the memory of the application. The
 * dumper copies what such a draw reads and stores the copy in the call (see GLClientArraySnapshot); each array of it, and the indices, are
 * resources of the model that are made once for the same bytes (see GLBlobStore), whatever the frame they are seen in.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_ARRAY_DRAW_DESCRIBER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_ARRAY_DRAW_DESCRIBER_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_blob_store.h"
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

                /* The draw that the call makes, with what the pipeline has set now; the description is only written if the result is DRAW. */
                GLArrayDrawResult describe(const Capture::APIDumpLoader::DumpedAPICall&        in_call,
                                           const Capture::APIDumpLoader::Workload&             in_workload,
                                           const bool&                                         in_is_indexed,
                                           const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                           const GLPipelineState&                              in_pipeline_state,
                                           Analysis::APIDumpAnalysis::DrawDescription*         out_description_ptr);

                /* The data that draws have read from the memory of the application, as resources. */
                const GLBlobStore& get_blob_store() const;

                /* The frame is over: the data that was read in it only is let go of. */
                void on_frame_ended();

            private:
                /* The sample of the geometry of the draw, from the array of the positions; null if none of the vertices could be read. */
                std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> make_geometry_sample(const std::vector<GLClientArray>&        in_array_vec,
                                                                                                      const Capture::APIDumpLoader::DataChunk* in_indices_chunk_ptr,
                                                                                                      const uint32_t&                          in_index_type,
                                                                                                      const int32_t&                           in_first,
                                                                                                      const int32_t&                           in_count) const;

                /* The index of the given place in the indices (of the type given, which is one of the three that glDrawElements takes). */
                static uint32_t read_index(const Capture::APIDumpLoader::DataChunk& in_indices_chunk,
                                           const uint32_t&                          in_index_type,
                                           const size_t&                            in_place);

                GLBlobStore                              m_blob_store;
                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_ARRAY_DRAW_DESCRIBER_H */

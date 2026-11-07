/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Follows the calls of an OpenGL application (GL, WGL and SwapBuffers) and tells the model builder what they come to.
 *
 * What it follows: the contexts and which one each thread has current, the viewport and the scissor box and test of each, the
 * values that a clear uses, the clears, and the end of the frames. What it does not follow yet (draws, copies and reads of the
 * framebuffer) is counted as unsupported, so that a model made of it says that its passes are not right.
 *
 * The window is the default framebuffer: one of them for the whole workload, made at the first clear with the size of the viewport that
 * the context has by then (the size of the window is not in the calls; a context starts with a viewport of the whole window, and the
 * application says what it is in its first glViewport).
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H

#include "analysis/api_dump_analysis/frontends/OpenGL/types.h"
#include "analysis/api_dump_analysis/frontends/shared/i_api_frontend.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "common/argument_reader.h"
#include <array>
#include <map>
#include <stddef.h>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class OpenGLFrontend : public IAPIFrontend
            {
            public:
                /* The builder must outlive the front end. */
                explicit OpenGLFrontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                bool find_resource_view(const uint64_t&                          in_address,
                                        Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const override;

                const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& get_n_resolution_hazards() const override;

                uint64_t get_n_unfollowed_calls() const override;

                uint64_t get_n_unsupported_calls() const override;

                void on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                             const Capture::APIDumpLoader::Workload&      in_workload) override;

            private:
                /* Makes the resources of the window, and tells the builder what is rendered to and where, if that has not been done. */
                void ensure_default_framebuffer(const GlContextState& in_state);

                /* The state of the context that the thread has current; nullptr if it has none. */
                GlContextState* find_current_context_ptr(const uint32_t& in_thread_id);

                void handle_clear        (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GlContextState*                              inout_state_ptr);
                void handle_clear_color  (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GlContextState*                              inout_state_ptr);
                void handle_clear_depth  (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GlContextState*                              inout_state_ptr);
                void handle_clear_stencil(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GlContextState*                              inout_state_ptr);
                void handle_enable       (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const bool&                                  in_is_enabled,
                                          GlContextState*                              inout_state_ptr);
                void handle_make_current (const Capture::APIDumpLoader::DumpedAPICall& in_call);
                void handle_scissor      (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GlContextState*                              inout_state_ptr);
                void handle_viewport     (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GlContextState*                              inout_state_ptr);

                /* The viewport of the model, from the box of the context. */
                static Analysis::APIDumpAnalysis::Viewport get_model_viewport(const GlContextState& in_state);

                /* True for the calls whose effects the front end does not follow yet. */
                static bool is_unsupported_function(const APIInterceptor::APIFunction& in_function);

                /* Reads the first N arguments of the call as numbers; false if it has fewer, or one that is not a number. */
                template<typename T, size_t N>
                static bool read_arguments(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           std::array<T, N>*                            out_value_array_ptr)
                {
                    bool result = (in_call.arg_in_vec.size() >= N);

                    for (size_t n_value = 0;
                                result   &&
                                n_value < N;
                              ++n_value)
                    {
                        result = APIInterceptor::read_number(in_call.arg_in_vec.at(n_value),
                                                             &out_value_array_ptr->at(n_value) );
                    }

                    return result;
                }

                /* A box from the four arguments of glViewport or glScissor. */
                static bool read_box(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     GlBox*                                       out_box_ptr);

                Analysis::APIDumpAnalysis::ModelBuilder*                        m_builder_ptr;
                Analysis::APIDumpAnalysis::ResourceView                         m_color_view;
                std::map<uint64_t, GlContextState>                              m_context_map;                   /* by the address of the context */
                std::map<uint32_t, uint64_t>                                    m_current_context_by_thread_map; /* the address, by the thread    */
                Analysis::APIDumpAnalysis::ResourceView                         m_depth_stencil_view;
                uint32_t                                                        m_framebuffer_height;
                uint32_t                                                        m_framebuffer_width;
                bool                                                            m_has_default_framebuffer;
                std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t> m_n_resolution_hazard_map;
                uint64_t                                                        m_n_unfollowed_calls;
                uint64_t                                                        m_n_unsupported_calls;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H */

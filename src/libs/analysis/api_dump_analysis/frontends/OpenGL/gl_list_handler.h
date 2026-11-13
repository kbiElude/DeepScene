/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the front end of OpenGL does with display lists: the lists of the program (the contexts share them), the calls that are kept in a list while it is made (with
 * the data that they refer to, so that the list keeps it after the workload that it came in is let go of), what a list does to what an eye has of its own, and the
 * calls of a list that is called. The calls of a called list are made by the front end, as the calls of the program are: the handler is given how, since a list may
 * call another.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_LIST_HANDLER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_LIST_HANDLER_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_context_state.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include <functional>
#include <map>
#include <memory>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLListHandler
            {
            public:
                /* How the front end makes a call: a call of the program, or one that a list keeps. */
                typedef std::function<void(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           const Capture::APIDumpLoader::Workload&      in_workload)> CallFunc;

                GLListHandler();

                /* What the list does to what an eye has of its own; false if there is no such list. */
                bool find_list_eye_dependence(const uint32_t&      in_list,
                                              GLListEyeDependence* out_dependence_ptr) const;

                /* How many of the calls about lists are not followed, and the front end goes on without them. */
                uint64_t get_n_unfollowed_calls() const;

                /* A call of glCallList: the calls of the list are made, as the list has them. */
                void handle_call_list(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const CallFunc&                              in_call_func);

                /* The lists of glDeleteLists are gone. */
                void handle_delete_lists(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                /* A list which was being made is made, and its calls are made when it is called. */
                void handle_end_list(GLContextState* inout_state_ptr);

                /* A call of glNewList: the calls made until glEndList are kept in the list, and made too if the mode says so. */
                void handle_new_list(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     GLContextState*                              inout_state_ptr);

                /* Keeps the call in the list that the context is making, with the data that it refers to. */
                void record_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                 const Capture::APIDumpLoader::Workload&      in_workload,
                                 GLContextState*                              inout_state_ptr);

            private:
                /* What a call that a list keeps does to what an eye has of its own: that of the list, for the call of one. */
                GLListEyeDependence get_call_eye_dependence(const APIInterceptor::APIFunction&                      in_function,
                                                            const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec) const;

                std::map<uint32_t, std::shared_ptr<GLDisplayList> > m_display_list_map; /* by the number of the list; the contexts share them */
                uint32_t                                            m_list_depth;       /* of the lists that are being called */
                uint64_t                                            m_n_unfollowed_calls;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_LIST_HANDLER_H */

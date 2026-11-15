/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Handles the OpenGL display lists: the lists of the program (shared by all contexts), the calls that are stored in a list while it is recorded (together
 * with the data they refer to, so that the list keeps the data after the workload that held it is released), whether a list depends on the eye, and the
 * calls of a list when it is called. The front end executes the calls of a called list in the same way as the calls of the program. The handler receives
 * the function that does this, because a list may call another list.
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
                /* How the front end executes a call, whether the call comes from the program or from a stored list. */
                typedef std::function<void(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           const Capture::APIDumpLoader::Workload&      in_workload)> CallFunc;

                GLListHandler();

                /* Returns whether the list depends on the eye (see GLListEyeDependence). Returns false if there is no such list. */
                bool find_list_eye_dependence(const uint32_t&      in_list,
                                              GLListEyeDependence* out_dependence_ptr) const;

                /* The number of list calls that are not followed. The front end continues without them. */
                uint64_t get_n_unfollowed_calls() const;

                /* A glCallList call: the calls stored in the list are executed. */
                void handle_call_list(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const CallFunc&                              in_call_func);

                /* A glDeleteLists call: the lists are deleted. */
                void handle_delete_lists(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                /* A glEndList call: the list being recorded is complete. Its calls are executed whenever it is called. */
                void handle_end_list(GLContextState* inout_state_ptr);

                /* A glNewList call: the calls made until glEndList are stored in the list, and also executed if the mode says so. */
                void handle_new_list(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     GLContextState*                              inout_state_ptr);

                /* Stores the call, with the data it refers to, in the list that the context is recording. */
                void record_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                 const Capture::APIDumpLoader::Workload&      in_workload,
                                 GLContextState*                              inout_state_ptr);

            private:
                /* Returns whether a stored call depends on the eye. For a call of another list, this is the dependence of that list. */
                GLListEyeDependence get_call_eye_dependence(const APIInterceptor::APIFunction&                      in_function,
                                                            const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec) const;

                std::map<uint32_t, std::shared_ptr<GLDisplayList> > m_display_list_map; /* by list number; all contexts share them */
                uint32_t                                            m_list_depth;       /* nesting depth of the lists being called */
                uint64_t                                            m_n_unfollowed_calls;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_LIST_HANDLER_H */

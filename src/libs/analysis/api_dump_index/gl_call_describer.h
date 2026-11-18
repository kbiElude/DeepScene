/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Describes the calls of OpenGL. Calls between glNewList and glEndList make the list and are state.
 */
#ifndef APIDUMPINDEX_GL_CALL_DESCRIBER_H
#define APIDUMPINDEX_GL_CALL_DESCRIBER_H

#include "analysis/api_dump_index/i_call_describer.h"

namespace Analysis
{
    namespace APIDumpIndex
    {
        class GLCallDescriber : public ICallDescriber
        {
        public:
            GLCallDescriber();

            CallClass describe(const Capture::APIDumpLoader::DumpedAPICall& in_call) override;
            Capture::APIDumpLoader::APIGroup  get_group() const override;

            /* Whether a display list is being recorded: between glNewList and glEndList. */
            bool is_recording() const;

            /* The class of the function outside a recorded list. */
            static CallClass get_function_class(const APIInterceptor::APIFunction& in_function);

        private:
            bool m_is_recording; /* between glNewList and glEndList */
        };
    }
}

#endif /* APIDUMPINDEX_GL_CALL_DESCRIBER_H */

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A D3D8 call as the dumper stored it, read by argument number: the arguments of the method first, then the data the dumper
 * added (what a pointer to a structure points to). A missing or wrongly typed argument reads as 0 or no data, so a damaged
 * call does not stop the analysis.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_CALL_VIEW_H
#define APIDUMPANALYSIS_FRONTENDS_CALL_VIEW_H

#include "analysis/api_dump_analysis/types.h"
#include "Capture/APIDumpLoader/api_dump_loader.h"
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class CallView
            {
            public:
                typedef std::vector<APIInterceptor::APIFunctionArgument> ArgumentVector;

                CallView(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                         const Capture::APIDumpLoader::Workload&      in_workload,
                         const size_t&                                in_n_head_arguments);

                const Capture::APIDumpLoader::DumpedAPICall& get_call() const;

                /* The data stored in place of a pointer-to-structure argument; nullptr if there is none. */
                const Capture::APIDumpLoader::DataChunk* get_chunk_ptr(const size_t& in_n_argument) const;

                /* Where the data of an argument is in the workload, and its hash. False if there is none. */
                bool get_data_reference(const size_t&                             in_n_argument,
                                        Analysis::APIDumpAnalysis::DataReference* out_reference_ptr) const;

                /* As above, for the data that follows the arguments of the method (the first such item is 0). */
                const Capture::APIDumpLoader::DataChunk* get_extra_chunk_ptr(const size_t& in_n_extra) const;

                float get_f32(const size_t& in_n_argument) const;

                /* For the arguments that are a BOOL. */
                int32_t get_i32(const size_t& in_n_argument) const;

                /* The number of arguments of the method itself. */
                size_t get_n_head_arguments() const;

                /* The data of an argument that the call has written (it is in the out arguments, e.g. a locked rectangle). */
                const Capture::APIDumpLoader::DataChunk* get_out_chunk_ptr(const size_t& in_n_argument) const;

                /* The value that an argument which is a pointer to a number has after the call (e.g. the handle of a shader). */
                uint32_t get_out_u32(const size_t& in_n_argument) const;

                /* The address that a pointer argument has after the call: for an argument which the call writes an object to, that
                 * of the object.
                 */
                uint64_t get_out_pointer(const size_t& in_n_argument) const;

                /* The address that a pointer argument has. */
                uint64_t get_pointer(const size_t& in_n_argument) const;

                uint64_t get_returned_pointer() const;
                uint32_t get_returned_u32    () const;

                uint32_t get_u32(const size_t& in_n_argument) const;

                /* True if the method returned a HRESULT which is not a failure. */
                bool succeeded() const;

            private:
                /* The data of the argument, if it is data. */
                const Capture::APIDumpLoader::DataChunk* find_chunk_ptr(const ArgumentVector& in_argument_vec,
                                                                        const size_t&         in_n_argument) const;


                /* The address that the argument has, if it is a pointer. */
                static uint64_t find_pointer(const ArgumentVector& in_argument_vec,
                                             const size_t&         in_n_argument);


                const Capture::APIDumpLoader::DumpedAPICall& m_call;
                const size_t                                 m_n_head_arguments;
                const Capture::APIDumpLoader::Workload&      m_workload;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_CALL_VIEW_H */

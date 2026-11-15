/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A D3D8 call as the dumper stored it, read by argument number: first the arguments of the method, then the data that the dumper added
 * (what a pointer to a structure points to). A missing or wrongly typed argument reads as 0 or as no data, so a damaged call does not stop
 * the analysis.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_CALL_VIEW_H
#define APIDUMPANALYSIS_FRONTENDS_CALL_VIEW_H

#include "analysis/api_dump_analysis/types.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include "common/api_function_argument.h"
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

                /* The data stored in place of an argument that is a pointer to a structure; nullptr if there is none. */
                const Capture::APIDumpLoader::DataChunk* get_chunk_ptr(const size_t& in_n_argument) const;

                /* Where the data of an argument is in the workload, and its hash. Returns false if the argument has no data. */
                bool get_data_reference(const size_t&                             in_n_argument,
                                        Analysis::APIDumpAnalysis::DataReference* out_reference_ptr) const;

                /* Like get_chunk_ptr(), but for the data that follows the arguments of the method (numbered from 0). */
                const Capture::APIDumpLoader::DataChunk* get_extra_chunk_ptr(const size_t& in_n_extra) const;

                float get_f32(const size_t& in_n_argument) const;

                /* Use this for arguments of type BOOL. */
                int32_t get_i32(const size_t& in_n_argument) const;

                /* The number of arguments of the method itself. */
                size_t get_n_head_arguments() const;

                /* The data that the call wrote to an out argument (for example, a locked rectangle). */
                const Capture::APIDumpLoader::DataChunk* get_out_chunk_ptr(const size_t& in_n_argument) const;

                /* The number that an argument of type pointer-to-number holds after the call (for example, the handle of a shader). */
                uint32_t get_out_u32(const size_t& in_n_argument) const;

                /* The address that a pointer argument holds after the call. For an argument that the call writes an object to, this is the
                 * address of that object.
                 */
                uint64_t get_out_pointer(const size_t& in_n_argument) const;

                /* The address that a pointer argument holds. */
                uint64_t get_pointer(const size_t& in_n_argument) const;

                uint64_t get_returned_pointer() const;
                uint32_t get_returned_u32    () const;

                uint32_t get_u32(const size_t& in_n_argument) const;

                /* True if the method returned a HRESULT that is a failure. False if the method returns nothing or no HRESULT; succeeded() is false then too. */
                bool has_failed() const;

                /* True if the method returned a HRESULT that is not a failure. */
                bool succeeded() const;

            private:
                /* The data of the argument, or nullptr if the argument is not data. */
                const Capture::APIDumpLoader::DataChunk* find_chunk_ptr(const ArgumentVector& in_argument_vec,
                                                                        const size_t&         in_n_argument) const;


                /* The address of the argument, or 0 if the argument is not a pointer. */
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

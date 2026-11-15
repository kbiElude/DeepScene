/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes a workload file one segment at a time, in the layout that the dumper writes and the loader reads (see common/workload_format.h). The
 * compression is the dumper's (ZstdFileWriter): each segment is a separate Zstandard frame.
 */
#ifndef ANALYSIS_APIDUMPSLICING_WORKLOAD_WRITER_H
#define ANALYSIS_APIDUMPSLICING_WORKLOAD_WRITER_H

#include "capture/api_dump_loader/api_dump_loader.h"
#include "common/workload_format.h"
#include <memory>
#include <stdio.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpSlicing
    {
        class WorkloadWriter;

        typedef std::unique_ptr<WorkloadWriter> WorkloadWriterUniquePtr;

        class WorkloadWriter
        {
        public:
            /* Creates a new file. Returns nullptr, with the reason in the error, if the file cannot be created. */
            static WorkloadWriterUniquePtr create(const char*                                               in_filename_ptr,
                                                  const APIInterceptor::WorkloadFormat::WorkloadFileHeader& in_header,
                                                  std::string*                                              out_error_ptr);

            ~WorkloadWriter();

            /* Flushes and closes the file. Returns false if the file cannot be written. The destructor calls it if the file is still open. */
            bool close();

            /* The number of bytes written to the file so far. */
            uint64_t get_n_bytes_written() const;

            /* Writes a segment that is already a Zstandard frame, as stored in the file of another workload. Returns false if the file cannot be
             * written.
             */
            bool write_compressed_segment(const uint8_t* in_data_ptr,
                                          const size_t&  in_n_bytes);

            /* Compresses the segment into a Zstandard frame of its own, holding its calls, data chunks and surfaces. A data chunk that the segment
             * does not hold (it was skipped when the segment was read) is written empty. Returns false if the segment cannot be compressed or the
             * file cannot be written.
             */
            bool write_segment(const Capture::APIDumpLoader::Workload& in_segment);

        private:
            WorkloadWriter();

            FILE*    m_file_handle;
            uint64_t m_n_bytes_written;

            /* Appends the 32-bit values to the byte vector. */
            static void append_u32_values(std::vector<uint8_t>* inout_u8_vec_ptr,
                                          const uint32_t*       in_value_ptr,
                                          const size_t&         in_n_values);

            /* Appends the call in the format of the dumper: the header, the arguments, the results and the return value. A call without a return
             * value stores an argument of no type in its place.
             */
            static void serialize_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                       std::vector<uint8_t>*                        inout_u8_vec_ptr);
        };
    }
}

#endif /* ANALYSIS_APIDUMPSLICING_WORKLOAD_WRITER_H */

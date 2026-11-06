/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes a workload file: the layout that the dumper writes and the loader reads (see Common/workload_format.h), a segment at a time. The compression is the
 * dumper's (ZstdFileWriter): each segment is a Zstandard frame of its own.
 */
#ifndef ANALYSIS_APIDUMPSLICING_WORKLOAD_WRITER_H
#define ANALYSIS_APIDUMPSLICING_WORKLOAD_WRITER_H

#include "Capture/APIDumpLoader/APIDumpLoader.h"
#include "Common/workload_format.h"
#include <memory>
#include <stdio.h>
#include <string>

namespace Analysis
{
    namespace APIDumpSlicing
    {
        class WorkloadWriter;

        typedef std::unique_ptr<WorkloadWriter> WorkloadWriterUniquePtr;

        class WorkloadWriter
        {
        public:
            ~WorkloadWriter();

            /* Flushes and closes the file. False if it cannot be written. Called by the destructor too, if it was not. */
            bool close();

            /* The file is made new. Returns nullptr, and says why, if it cannot be. */
            static WorkloadWriterUniquePtr create(const char*                                               in_filename_ptr,
                                                  const APIInterceptor::WorkloadFormat::WorkloadFileHeader& in_header,
                                                  std::string*                                              out_error_ptr);

            /* The bytes that the file has so far. */
            uint64_t get_n_bytes_written() const;

            /* A segment that is already a Zstandard frame, as the file of another workload has it. False if the file cannot be written. */
            bool write_compressed_segment(const uint8_t* in_data_ptr,
                                          const size_t&  in_n_bytes);

            /* The segment as a frame of its own: its calls, data chunks and surfaces. A data chunk which the segment does not have (it was left out when
             * it was read) is written empty. False if it cannot be compressed, or the file cannot be written.
             */
            bool write_segment(const Capture::APIDumpLoader::Workload& in_segment);

        private:
            WorkloadWriter();

            FILE*    m_file_handle;
            uint64_t m_n_bytes_written;
        };
    }
}

#endif /* ANALYSIS_APIDUMPSLICING_WORKLOAD_WRITER_H */

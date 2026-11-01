/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_CALL_WALKER_H
#define STEREO_REPLAY_ENGINEDLL_CALL_WALKER_H

#include "Capture/APIDumpLoader/APIDumpLoader.h"
#include "Capture/APIDumpLoader/WorkloadReader.h"
#include "i_chunk_source.h"
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Walks the calls of a workload file in the order they were made. A thread of its own reads the next segment (and frees
             * the previous one) while the current one is replayed, so the replay does not wait for the disk.
             */
            class CallWalker : public IChunkSource
            {
            public:
                 CallWalker();
                ~CallWalker();

                /* Returns nullptr for a chunk which is not part of the segment of the current call. */
                const Capture::APIDumpLoader::DataChunk* get_data_chunk_ptr(const uint32_t& in_data_chunk_id) const override;

                uint32_t get_pointer_n_bytes() const override;

                const Capture::APIDumpLoader::Workload& get_segment() const;

                /* Opens the file. */
                bool open(const char*  in_filename_ptr,
                          std::string* out_error_ptr);

                /* False once there is no call left. The call (and its chunks) stay valid until the next call of this function. */
                bool read_next_call(const Capture::APIDumpLoader::DumpedAPICall** out_call_ptr_ptr);

                /* Goes back to the start of the file opened before. */
                bool rewind(std::string* out_error_ptr);

            private:
                /* The segment which the thread has read ahead, shared with it. */
                struct Prefetch
                {
                    std::condition_variable          cv;
                    bool                             is_finished       = false; /* the file has no segment left                         */
                    bool                             is_ready          = false; /* segment is the next one; otherwise, one to destroy */
                    bool                             is_stop_requested = false;
                    std::mutex                       mutex;
                    Capture::APIDumpLoader::Workload segment;
                };

                void prefetch_main    ();
                bool read_next_segment();
                void stop_prefetching ();

                std::string                                     m_filename;
                size_t                                          m_n_next_call;
                Prefetch                                        m_prefetch;
                std::thread                                     m_prefetch_thread;
                Capture::APIDumpLoader::WorkloadReaderUniquePtr m_reader_ptr;
                Capture::APIDumpLoader::Workload                m_segment;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_CALL_WALKER_H */

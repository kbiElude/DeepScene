/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "call_walker.h"

Stereo::Replay::EngineDLL::CallWalker::CallWalker()
    :m_n_next_call(0)
{
    /* Stub */
}

Stereo::Replay::EngineDLL::CallWalker::~CallWalker()
{
    stop_prefetching();
}

const Capture::APIDumpLoader::DataChunk* Stereo::Replay::EngineDLL::CallWalker::get_data_chunk_ptr(const uint32_t& in_data_chunk_id) const
{
    return Capture::APIDumpLoader::get_data_chunk_ptr(m_segment,
                                                      in_data_chunk_id);
}

uint32_t Stereo::Replay::EngineDLL::CallWalker::get_pointer_n_bytes() const
{
    return m_segment.pointer_n_bytes;
}

const Capture::APIDumpLoader::Workload& Stereo::Replay::EngineDLL::CallWalker::get_segment() const
{
    return m_segment;
}

bool Stereo::Replay::EngineDLL::CallWalker::open(const char*  in_filename_ptr,
                                                 std::string* out_error_ptr)
{
    std::string                                     error;
    Capture::APIDumpLoader::WorkloadReaderUniquePtr reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(in_filename_ptr,
                                                                                                                &error);
    bool                                            result     = false;

    if (reader_ptr == nullptr)
    {
        if (out_error_ptr != nullptr)
        {
            *out_error_ptr = error;
        }

        goto end;
    }

    stop_prefetching();

    m_filename    = in_filename_ptr;
    m_n_next_call = 0;
    m_reader_ptr  = std::move(reader_ptr);
    m_segment     = Capture::APIDumpLoader::Workload();

    {
        std::lock_guard<std::mutex> lock(m_prefetch.mutex);

        m_prefetch.is_finished       = false;
        m_prefetch.is_ready          = false;
        m_prefetch.is_stop_requested = false;
        m_prefetch.segment           = Capture::APIDumpLoader::Workload();
    }

    m_prefetch_thread = std::thread(&CallWalker::prefetch_main,
                                    this);
    result            = true;

end:
    return result;
}

void Stereo::Replay::EngineDLL::CallWalker::prefetch_main()
{
    bool is_done = false;

    while (is_done == false)
    {
        Capture::APIDumpLoader::Workload next_segment;
        Capture::APIDumpLoader::Workload old_segment;
        bool                             is_read = false;

        {
            std::unique_lock<std::mutex> lock(m_prefetch.mutex);

            m_prefetch.cv.wait(lock,
                               [this]()
                               {
                                   return (m_prefetch.is_ready == false ||
                                           m_prefetch.is_stop_requested);
                               });

            if (m_prefetch.is_stop_requested)
            {
                break;
            }

            /* What is left in the slot is the segment which the replay has finished with. */
            old_segment = std::move(m_prefetch.segment);
        }

        /* Free the memory of the old segment (it takes a while), then read the next one. */
        old_segment = Capture::APIDumpLoader::Workload();
        is_read     = m_reader_ptr->read_next_segment (&next_segment);

        {
            std::lock_guard<std::mutex> lock(m_prefetch.mutex);

            if (m_prefetch.is_stop_requested)
            {
                break;
            }

            if (is_read)
            {
                m_prefetch.segment  = std::move(next_segment);
                m_prefetch.is_ready = true;
            }
            else
            {
                m_prefetch.is_finished = true;
            }
        }

        m_prefetch.cv.notify_all();

        is_done = (is_read == false);
    }
}

bool Stereo::Replay::EngineDLL::CallWalker::read_next_call(const Capture::APIDumpLoader::DumpedAPICall** out_call_ptr_ptr)
{
    bool result = false;

    if (m_reader_ptr == nullptr)
    {
        goto end;
    }

    /* A segment may hold no calls; skip those. */
    while (m_segment.dumped_api_call_vec_ptr == nullptr                                   ||
           m_n_next_call                     >= m_segment.dumped_api_call_vec_ptr->size() )
    {
        if (read_next_segment() == false)
        {
            goto end;
        }
    }

    *out_call_ptr_ptr = &m_segment.dumped_api_call_vec_ptr->at(m_n_next_call);
    m_n_next_call++;

    result = true;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::CallWalker::read_next_segment()
{
    bool result = false;

    m_n_next_call = 0;

    {
        std::unique_lock<std::mutex> lock(m_prefetch.mutex);

        m_prefetch.cv.wait(lock,
                           [this]()
                           {
                               return (m_prefetch.is_ready ||
                                       m_prefetch.is_finished);
                           });

        if (m_prefetch.is_ready)
        {
            /* The old segment goes to the thread, which destroys it. */
            std::swap(m_segment,
                      m_prefetch.segment);

            m_prefetch.is_ready = false;
            result              = true;
        }
    }

    m_prefetch.cv.notify_all();

    return result;
}

bool Stereo::Replay::EngineDLL::CallWalker::rewind(std::string* out_error_ptr)
{
    const std::string filename = m_filename;

    return open(filename.c_str(),
                out_error_ptr);
}

void Stereo::Replay::EngineDLL::CallWalker::stop_prefetching()
{
    {
        std::lock_guard<std::mutex> lock(m_prefetch.mutex);

        m_prefetch.is_stop_requested = true;
    }

    m_prefetch.cv.notify_all();

    if (m_prefetch_thread.joinable() )
    {
        m_prefetch_thread.join();
    }
}

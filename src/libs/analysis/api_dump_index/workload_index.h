/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Locates the frames of a workload in its file: per segment (a Zstandard frame) its offset, call count and frames, so a tool reads only
 * the segments it needs (see WorkloadReader::Options::start_offset_n_bytes). Each segment also has a state frame with the calls that
 * a replay of unneeded frames still makes (see CallDescriberSet, StateSegmentBuilder); a reader uses it when only those calls are wanted.
 *
 * Frames count from 0 as the printer counts them; a frame ends with a call for which APIDumpLoader::is_frame_boundary() is true.
 * The index is built by one read of the workload and kept in a file next to it. It records the workload's size and 4 KiB hashes
 * of its start, middle and end, and is not used for another file.
 */
#ifndef APIDUMPINDEX_WORKLOAD_INDEX_H
#define APIDUMPINDEX_WORKLOAD_INDEX_H

#include "capture/api_dump_loader/workload_reader.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpIndex
    {
        /* One segment of the workload. */
        struct SegmentIndexEntry
        {
            uint64_t file_offset_n_bytes = 0; /* of its frame in the file                      */
            uint64_t first_call_index    = 0; /* in the whole workload                         */
            uint64_t first_frame_index   = 0; /* frame of its first call: frames ended before it */
            uint32_t first_data_chunk_id = 0; /* data chunks before it                         */
            uint32_t n_calls             = 0;
            uint64_t n_compressed_bytes  = 0;
            uint32_t n_data_chunks       = 0;
            uint32_t n_frames_ended      = 0; /* its calls that end a frame                     */
            uint64_t state_n_bytes        = 0; /* of its state frame; 0: none                   */
            uint64_t state_offset_n_bytes = 0; /* of its state frame, from the first state frame */
        };

        class WorkloadIndex
        {
        public:
            WorkloadIndex();

            /* The workload's name plus a suffix. */
            static std::string get_default_filename(const std::string& in_workload_filename);

            /* Loads the index file, or builds and saves it if missing or of another (or a changed) workload. A failed save is not an error. */
            static bool load_or_build(const char*    in_workload_filename_ptr,
                                      const uint32_t& in_n_threads,
                                      WorkloadIndex*  out_index_ptr,
                                      bool*           out_was_built_ptr,
                                      std::string*    out_error_ptr);

            /* Builds the index by reading the workload segment by segment. */
            static bool build(const char*     in_workload_filename_ptr,
                              const uint32_t& in_n_threads,
                              WorkloadIndex*  out_index_ptr,
                              std::string*    out_error_ptr);

            /* False (with the reason) if the file is not an index of this version. */
            static bool load(const char*    in_index_filename_ptr,
                             WorkloadIndex* out_index_ptr,
                             std::string*   out_error_ptr);

            /* Segment holding the frame's first call (an earlier one if the frame started there); false if there is no such frame. */
            bool find_segment_of_frame(const uint64_t& in_frame_index,
                                       size_t*         out_n_segment_ptr) const;

            uint64_t get_n_calls() const;

            /* Ended frames, plus the open one if calls follow the last. */
            uint64_t get_n_frames() const;

            const std::vector<SegmentIndexEntry>& get_segment_vec() const;

            /* True if the index was built from this file in its current state. */
            bool is_of(const char* in_workload_filename_ptr) const;

            /* True if the file ended mid-segment; the rest was not read. */
            bool is_truncated() const;

            /* in_options with the start offset and data chunk count of the segment. */
            Capture::APIDumpLoader::WorkloadReader::Options make_reader_options(const size_t&                   in_n_segment,
                                                        const Capture::APIDumpLoader::WorkloadReader::Options& in_options) const;

            /* For WorkloadReader::Options::segment_start_vec, including state frame locations. */
            std::vector<Capture::APIDumpLoader::WorkloadReader::SegmentStart> make_segment_start_vec() const;

            /* False (with the reason) if the file cannot be written. */
            bool save(const char*  in_index_filename_ptr,
                      std::string* out_error_ptr) const;

        private:
            /* Size and hashes used by is_of(). */
            struct Identity
            {
                uint64_t head_hash = 0;
                uint64_t middle_hash = 0;
                uint64_t n_bytes   = 0;
                uint64_t tail_hash = 0;
            };

            /* False if the file cannot be read. */
            static bool get_identity(const char* in_workload_filename_ptr,
                                     Identity*   out_identity_ptr);

            Identity                       m_identity;
            bool                           m_is_truncated;
            uint64_t                       m_n_calls;
            uint64_t                       m_n_frames;
            std::vector<SegmentIndexEntry> m_segment_vec;
            std::vector<uint8_t>           m_state_byte_vec; /* state frames, between build() and save() */
        };
    }
}

#endif /* APIDUMPINDEX_WORKLOAD_INDEX_H */

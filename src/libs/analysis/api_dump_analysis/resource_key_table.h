/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The match keys of the resources of a frame: the key of how a resource was created, and the key of the pass that wrote to it in this frame,
 * if one did. A texture read by post-processing is therefore identified by what drew it. A resource that has not been written in the frame
 * (it comes from an earlier frame, or from an upload) has only the first key, so the keys of a frame do not depend on the earlier frames.
 */
#ifndef APIDUMPANALYSIS_RESOURCE_KEY_TABLE_H
#define APIDUMPANALYSIS_RESOURCE_KEY_TABLE_H

#include "analysis/api_dump_analysis/types.h"
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class ResourceKeyTable
        {
        public:
            ResourceKeyTable();

            /* Forgets which passes wrote to the resources: a new frame starts. */
            void begin_frame();

            void forget_resource(const ResourceId& in_id);

            /* Returns 0 for a resource that has not been registered. */
            uint64_t get_key(const ResourceId& in_id) const;

            /* The key that the resource would have if no pass of the frame had written to it. A pass that clears what it renders to does not
             * depend on what was there before. Returns 0 for a resource that has not been registered.
             */
            uint64_t get_unwritten_key(const ResourceId& in_id) const;

            uint64_t get_unwritten_view_key(const ResourceView& in_view) const;

            /* The key of a part of a resource (a mip level or a layer). */
            uint64_t get_view_key(const ResourceView& in_view) const;

            /* Records that the pass with the given key wrote to the resource. */
            void record_write(const ResourceId& in_id,
                              const uint64_t&   in_writer_pass_key);

            void register_resource(const ResourceId& in_id,
                                   const uint64_t&   in_description_key);

        private:
            /* The keys are computed when the data they depend on changes, because they are requested far more often than they change. */
            struct Entry
            {
                uint64_t description_key = 0;
                bool     has_writer      = false;
                bool     is_registered   = false;
                uint64_t key             = 0; /* includes the writer, if there is one */
                uint64_t unwritten_key   = 0;
                uint64_t writer_pass_key = 0;
            };

            /* Null for a resource that has not been registered. */
            const Entry* get_entry_ptr(const ResourceId& in_id) const;


            static uint64_t compute_key(const uint64_t& in_description_key,
                                        const bool&     in_has_writer,
                                        const uint64_t& in_writer_pass_key);

            std::vector<Entry>    m_entry_vec;           /* indexed by ID - 1                      */
            std::vector<uint32_t> m_written_index_vec;   /* the entries that have a writer now     */
        };
    }
}

#endif /* APIDUMPANALYSIS_RESOURCE_KEY_TABLE_H */

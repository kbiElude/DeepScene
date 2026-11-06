/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The match keys of the resources of a frame: the key of how a resource was made, and the key of the pass that wrote to it
 * in this frame, if one did, so a texture read by post-processing is told by what drew it. A resource not written yet in
 * the frame (from an earlier frame, or an upload) has the first key alone, so the keys of a frame do not depend on the ones
 * before.
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

            /* Forgets who has written to the resources: a new frame starts. */
            void begin_frame();

            void forget_resource(const ResourceId& in_id);

            /* Returns 0 for a resource which has not been registered. */
            uint64_t get_key(const ResourceId& in_id) const;

            /* The key the resource would have if no pass of the frame had written to it: a pass that clears what it renders to
             * does not depend on what was there. Returns 0 for a resource which has not been registered.
             */
            uint64_t get_unwritten_key(const ResourceId& in_id) const;

            uint64_t get_unwritten_view_key(const ResourceView& in_view) const;

            /* The key of a part of a resource (a mip level, a layer). */
            uint64_t get_view_key(const ResourceView& in_view) const;

            /* The pass that has the key has written to the resource. */
            void record_write(const ResourceId& in_id,
                              const uint64_t&   in_writer_pass_key);

            void register_resource(const ResourceId& in_id,
                                   const uint64_t&   in_description_key);

        private:
            /* The keys are worked out when what they depend on changes, since they are asked for far more often. */
            struct Entry
            {
                uint64_t description_key = 0;
                bool     has_writer      = false;
                bool     is_registered   = false;
                uint64_t key             = 0; /* with the writer, if there is one */
                uint64_t unwritten_key   = 0;
                uint64_t writer_pass_key = 0;
            };

            /* Null for a resource which has not been registered. */
            const Entry* get_entry_ptr(const ResourceId& in_id) const;


            static uint64_t compute_key(const uint64_t& in_description_key,
                                        const bool&     in_has_writer,
                                        const uint64_t& in_writer_pass_key);

            std::vector<Entry>    m_entry_vec;           /* by ID - 1                             */
            std::vector<uint32_t> m_written_index_vec;   /* the entries which have a writer now   */
        };
    }
}

#endif /* APIDUMPANALYSIS_RESOURCE_KEY_TABLE_H */

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Remembers the keys of the last few distinct draws. A draw whose description, orthographic flag, render targets and resource keys are those of a remembered
 * draw has the same keys, so the model builder does not work them out again. API-neutral: everything that decides a key is compared, nothing is assumed.
 */
#ifndef APIDUMPANALYSIS_DRAW_KEY_CACHE_H
#define APIDUMPANALYSIS_DRAW_KEY_CACHE_H

#include "analysis/api_dump_analysis/types.h"
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class DrawKeyCache
        {
        public:
            DrawKeyCache();

            /* True, and the remembered keys, if the draw is a remembered one. The key table epoch is ResourceKeyTable::get_epoch(): the keys of the resources have
             * not changed while it has not.
             */
            bool find(const DrawDescription& in_draw,
                      const AttachmentSet&   in_attachments,
                      const uint64_t&        in_key_table_epoch,
                      const bool&            in_is_orthographic,
                      DrawKeyResult*         out_result_ptr);

            /* Remembers the draw, forgetting the one remembered the longest ago if the cache is full. */
            void store(const DrawDescription& in_draw,
                       const AttachmentSet&   in_attachments,
                       const uint64_t&        in_key_table_epoch,
                       const bool&            in_is_orthographic,
                       const DrawKeyResult&   in_result);

        private:
            /* What decides the keys of a draw, and the keys. */
            struct Entry
            {
                AttachmentSet               attachments;
                ResourceId                  index_buffer_id    = ResourceId::NONE;
                bool                        is_indexed         = false;
                bool                        is_instanced       = false;
                bool                        is_orthographic    = false;
                bool                        is_used            = false;
                uint64_t                    key_table_epoch    = 0;
                PrimitiveTopology           primitive_topology = PrimitiveTopology::UNKNOWN;
                DrawKeyResult               result;
                std::vector<ResourceId>     shader_id_vec;
                uint64_t                    state_key          = 0;
                std::vector<TextureBinding> texture_binding_vec;
                std::vector<ResourceId>     vertex_buffer_id_vec;
                uint64_t                    vertex_layout_key  = 0;
            };

            static bool is_match(const Entry&           in_entry,
                                 const DrawDescription& in_draw,
                                 const AttachmentSet&   in_attachments,
                                 const uint64_t&        in_key_table_epoch,
                                 const bool&            in_is_orthographic);

            std::vector<Entry> m_entry_vec;
            size_t             m_n_last_hit_entry;
            size_t             m_n_next_victim_entry;
        };
    }
}

#endif /* APIDUMPANALYSIS_DRAW_KEY_CACHE_H */

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Keeps one copy of each structure that a capture repeats, and numbers the copies. A capture has hundreds of thousands
 * of frames made of a few dozen different structures, so the analysis refers to a structure by its number instead of
 * keeping it for every frame.
 */
#ifndef APIDUMPANALYSIS_STRUCTURE_INTERNER_H
#define APIDUMPANALYSIS_STRUCTURE_INTERNER_H

#include "Analysis/APIDumpAnalysis/types.h"
#include <stdint.h>
#include <unordered_map>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        /* Structures are told apart by comparing them (they need an operator==); the key only narrows the choice, so that a
         * key that two different structures share is harmless.
         *
         * The functions are defined in structure_interner.cpp, for FrameShape and PassStructure, so that they are not built
         * in every file that uses an interner. A structure of another type needs an explicit instantiation there.
         */
        template<typename Structure>
        class StructureInterner
        {
        public:
            StructureInterner();

            const Structure& get_structure   (const uint32_t& in_id) const;
            size_t           get_n_structures()                      const;

            /* Returns the number of the structure (from 0), which is a new one if none of the structures with the key is
             * equal to it.
             */
            uint32_t intern(const Structure& in_structure,
                            const uint64_t&  in_key,
                            bool*            out_is_new_ptr = nullptr);

        private:
            std::unordered_multimap<uint64_t, uint32_t> m_id_map; /* by key */
            std::vector<Structure>                      m_structure_vec;
        };
    }
}

#endif /* APIDUMPANALYSIS_STRUCTURE_INTERNER_H */

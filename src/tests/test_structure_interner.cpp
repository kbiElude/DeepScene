/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of keeping one copy of each repeated structure.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/structure_interner.h"

TEST(StructureInterner, EqualStructuresShareANumber)
{
    Analysis::APIDumpAnalysis::StructureInterner<Analysis::APIDumpAnalysis::FrameShape> interner;
    bool                                                                                is_new = false;

    const uint32_t first = interner.intern(Analysis::APIDumpAnalysis::FrameShape{{1, 2, 3}},
                                           100,
                                          &is_new);

    EXPECT_TRUE(is_new);
    EXPECT_EQ  (first,
                0u);

    const uint32_t again = interner.intern(Analysis::APIDumpAnalysis::FrameShape{{1, 2, 3}},
                                           100,
                                          &is_new);

    EXPECT_FALSE(is_new);
    EXPECT_EQ   (again,
                 first);
    EXPECT_EQ   (interner.get_n_structures(),
                 1u);
}

TEST(StructureInterner, DifferentStructuresGetNumbersInTheOrderTheyAppear)
{
    Analysis::APIDumpAnalysis::StructureInterner<Analysis::APIDumpAnalysis::FrameShape> interner;

    EXPECT_EQ(interner.intern(Analysis::APIDumpAnalysis::FrameShape{{1}},
                              1),
              0u);
    EXPECT_EQ(interner.intern(Analysis::APIDumpAnalysis::FrameShape{{2}},
                              2),
              1u);
    EXPECT_EQ(interner.intern(Analysis::APIDumpAnalysis::FrameShape{{3}},
                              3),
              2u);
    EXPECT_EQ(interner.intern(Analysis::APIDumpAnalysis::FrameShape{{2}},
                              2),
              1u);

    EXPECT_TRUE(interner.get_structure(1) == Analysis::APIDumpAnalysis::FrameShape{{2}});
}

TEST(StructureInterner, AHashTwoStructuresShareIsHarmless)
{
    Analysis::APIDumpAnalysis::StructureInterner<Analysis::APIDumpAnalysis::FrameShape> interner;

    const uint32_t a = interner.intern(Analysis::APIDumpAnalysis::FrameShape{{1, 1}},
                                       42);
    const uint32_t b = interner.intern(Analysis::APIDumpAnalysis::FrameShape{{2, 2}},
                                       42);

    EXPECT_NE(a,
              b);
    EXPECT_EQ(interner.intern(Analysis::APIDumpAnalysis::FrameShape{{1, 1}},
                              42),
              a);
    EXPECT_EQ(interner.intern(Analysis::APIDumpAnalysis::FrameShape{{2, 2}},
                              42),
              b);
    EXPECT_EQ(interner.get_n_structures(),
              2u);
}

TEST(StructureInterner, AMillionRepetitionsKeepFiveStructures)
{
    constexpr uint32_t N_REPETITIONS = 1000000;
    constexpr uint32_t N_STRUCTURES  = 5;

    Analysis::APIDumpAnalysis::StructureInterner<Analysis::APIDumpAnalysis::FrameShape> interner;

    for (uint32_t n_repetition = 0;
                  n_repetition < N_REPETITIONS;
                ++n_repetition)
    {
        const uint32_t kind = n_repetition % N_STRUCTURES;

        interner.intern(Analysis::APIDumpAnalysis::FrameShape{{kind, kind + 1}},
                        kind);
    }

    EXPECT_EQ(interner.get_n_structures(),
              static_cast<size_t>(N_STRUCTURES) );
}

TEST(StructureInterner, PassStructuresAreInternedToo)
{
    Analysis::APIDumpAnalysis::StructureInterner<Analysis::APIDumpAnalysis::PassStructure> interner;
    Analysis::APIDumpAnalysis::PassStructure                                               a;
    Analysis::APIDumpAnalysis::PassStructure                                               b;

    a.clears_color = true;

    EXPECT_NE(interner.intern(a,
                              1),
              interner.intern(b,
                              1) );
    EXPECT_EQ(interner.get_n_structures(),
              2u);
}

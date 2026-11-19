/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The twins of the textures of an OpenGL application: names below and above the limit of the array behave the same.
 */
#include <gtest/gtest.h>
#include "OpenGL/gl_texture_twin_table.h"
#include <map>

namespace
{
    /* A name inside the array, one at its edge, and two outside of it. */
    const uint32_t NAME_ARRAY[] = {1, 77, (1u << 18) - 1, 1u << 18, 4000000000u};
}

TEST(GLTextureTwinTable, ATableThatWasNeverSetIsEmptyAndFindsNothing)
{
    Stereo::ConverterDLL::GLTextureTwinTable table;
    uint32_t                                 twin = 99;

    EXPECT_TRUE (table.is_empty() );
    EXPECT_FALSE(table.find(5,
                           &twin) );
    EXPECT_FALSE(table.find(4000000000u,
                           &twin) );
}

TEST(GLTextureTwinTable, ATwinIsFoundUntilItIsErased)
{
    for (const auto name : NAME_ARRAY)
    {
        Stereo::ConverterDLL::GLTextureTwinTable table;
        uint32_t                                 twin = 0;

        table.set(name,
                  name + 1000);

        EXPECT_FALSE(table.is_empty() );
        ASSERT_TRUE (table.find(name,
                               &twin) );
        EXPECT_EQ   (twin,
                     name + 1000);
        EXPECT_FALSE(table.find(name + 1,
                               &twin) );

        table.erase(name);

        EXPECT_TRUE (table.is_empty() );
        EXPECT_FALSE(table.find(name,
                               &twin) );
    }
}

TEST(GLTextureTwinTable, SettingATextureAgainReplacesItsTwinAndCountsItOnce)
{
    Stereo::ConverterDLL::GLTextureTwinTable table;
    uint32_t                                 twin = 0;

    for (const auto name : NAME_ARRAY)
    {
        table.set(name,
                  10);
        table.set(name,
                  20);

        ASSERT_TRUE(table.find(name,
                              &twin) );
        EXPECT_EQ  (twin,
                    20u);
    }

    for (const auto name : NAME_ARRAY)
    {
        EXPECT_FALSE(table.is_empty() );

        table.erase(name);
        table.erase(name);
    }

    EXPECT_TRUE(table.is_empty() );
}

TEST(GLTextureTwinTable, ErasingATextureThatHasNoTwinChangesNothing)
{
    Stereo::ConverterDLL::GLTextureTwinTable table;
    uint32_t                                 twin = 0;

    table.set(7,
              70);
    table.erase(8);
    table.erase(4000000000u);
    table.erase(300000);

    EXPECT_FALSE(table.is_empty() );
    EXPECT_TRUE (table.find(7,
                           &twin) );
}

TEST(GLTextureTwinTable, TheTableIsAsGoodAsAMapOnAShuffledRun)
{
    Stereo::ConverterDLL::GLTextureTwinTable table;
    std::map<uint32_t, uint32_t>             reference;
    uint64_t                                 state = 99;

    for (uint32_t n_step = 0;
                  n_step < 20000;
                ++n_step)
    {
        state = state * 6364136223846793005ull + 1442695040888963407ull;

        const uint32_t name   = ((state >> 40) % 3 == 0) ? static_cast<uint32_t>((state >> 20) % 400000)
                                                         : static_cast<uint32_t>((state >> 20) % 60);
        const uint32_t action = static_cast<uint32_t>(state >> 60);

        if (action < 9)
        {
            table.set(name,
                      n_step + 1);

            reference[name] = n_step + 1;
        }
        else
        {
            table.erase(name);

            reference.erase(name);
        }

        uint32_t twin  = 0;
        const bool found = table.find(name,
                                     &twin);

        EXPECT_EQ(found,
                  reference.count(name) != 0);
        EXPECT_EQ(table.is_empty(),
                  reference.empty() );

        if (found)
        {
            EXPECT_EQ(twin,
                      reference.at(name) );
        }
    }
}

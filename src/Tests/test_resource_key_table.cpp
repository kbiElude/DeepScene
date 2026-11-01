/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the keys of the resources of a frame: they follow the data flow, not the order of the creation calls or the
 * addresses.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/key_builder.h"
#include "Analysis/APIDumpAnalysis/resource_key_table.h"
#include <algorithm>
#include <vector>

TEST(ResourceKeyTable, UnknownResourcesHaveNoKey)
{
    Analysis::APIDumpAnalysis::ResourceKeyTable table;

    EXPECT_EQ(table.get_key(Analysis::APIDumpAnalysis::make_resource_id(1) ),
              0u);

    table.register_resource(Analysis::APIDumpAnalysis::make_resource_id(1),
                            0x1234);

    EXPECT_NE(table.get_key(Analysis::APIDumpAnalysis::make_resource_id(1) ),
              0u);

    table.forget_resource(Analysis::APIDumpAnalysis::make_resource_id(1) );

    EXPECT_EQ(table.get_key(Analysis::APIDumpAnalysis::make_resource_id(1) ),
              0u);
}

TEST(ResourceKeyTable, KeysFollowTheDescriptionsNotTheIDs)
{
    Analysis::APIDumpAnalysis::ResourceKeyTable first_table;
    Analysis::APIDumpAnalysis::ResourceKeyTable second_table;

    /* The same resources, made in another order and with other IDs. */
    first_table.register_resource (Analysis::APIDumpAnalysis::make_resource_id(1),
                                   0xAAAA);
    first_table.register_resource (Analysis::APIDumpAnalysis::make_resource_id(2),
                                   0xBBBB);
    second_table.register_resource(Analysis::APIDumpAnalysis::make_resource_id(7),
                                   0xBBBB);
    second_table.register_resource(Analysis::APIDumpAnalysis::make_resource_id(3),
                                   0xAAAA);

    EXPECT_EQ(first_table.get_key(Analysis::APIDumpAnalysis::make_resource_id(1) ),
              second_table.get_key(Analysis::APIDumpAnalysis::make_resource_id(3) ));
    EXPECT_EQ(first_table.get_key(Analysis::APIDumpAnalysis::make_resource_id(2) ),
              second_table.get_key(Analysis::APIDumpAnalysis::make_resource_id(7) ));
    EXPECT_NE(first_table.get_key(Analysis::APIDumpAnalysis::make_resource_id(1) ),
              first_table.get_key(Analysis::APIDumpAnalysis::make_resource_id(2) ));
}

TEST(ResourceKeyTable, AWriteChangesTheKeyUntilTheNextFrame)
{
    const auto                                  id = Analysis::APIDumpAnalysis::make_resource_id(1);
    Analysis::APIDumpAnalysis::ResourceKeyTable table;

    table.register_resource(id,
                            0x1234);

    const uint64_t key_before_write = table.get_key(id);

    table.record_write(id,
                       0x5555);

    const uint64_t key_after_write = table.get_key(id);

    EXPECT_NE(key_after_write,
              key_before_write);

    table.record_write(id,
                       0x6666);

    EXPECT_NE(table.get_key(id),
              key_after_write);

    table.begin_frame();

    EXPECT_EQ(table.get_key(id),
              key_before_write);
}

TEST(ResourceKeyTable, TheSameWriterGivesTheSameKeyToResourcesOfOneDescription)
{
    const auto                                  id_a = Analysis::APIDumpAnalysis::make_resource_id(1);
    const auto                                  id_b = Analysis::APIDumpAnalysis::make_resource_id(2);
    Analysis::APIDumpAnalysis::ResourceKeyTable table;

    table.register_resource(id_a,
                            0x1234);
    table.register_resource(id_b,
                            0x1234);

    table.record_write(id_a,
                       0x5555);

    EXPECT_NE(table.get_key(id_a),
              table.get_key(id_b) );

    table.record_write(id_b,
                       0x5555);

    EXPECT_EQ(table.get_key(id_a),
              table.get_key(id_b) );
}

TEST(ResourceKeyTable, PingPongGivesTheSameKeysWhicheverResourceStartsIt)
{
    const auto id_a = Analysis::APIDumpAnalysis::make_resource_id(1);
    const auto id_b = Analysis::APIDumpAnalysis::make_resource_id(2);

    /* Each pass reads what the previous one has written, and writes to the other resource. The key of a pass is made of
     * what it reads.
     */
    constexpr uint32_t N_PASSES = 4;

    const auto run_chain_func = [&](const Analysis::APIDumpAnalysis::ResourceId& in_first,
                                    const Analysis::APIDumpAnalysis::ResourceId& in_second)
    {
        Analysis::APIDumpAnalysis::ResourceKeyTable table;
        std::vector<uint64_t>                       pass_key_vec;
        Analysis::APIDumpAnalysis::ResourceId       read_id  = in_first;
        Analysis::APIDumpAnalysis::ResourceId       write_id = in_second;

        table.register_resource(in_first,
                                0x1234);
        table.register_resource(in_second,
                                0x1234);

        for (uint32_t n_pass = 0;
                      n_pass < N_PASSES;
                    ++n_pass)
        {
            const uint64_t pass_key = Analysis::APIDumpAnalysis::KeyBuilder::compute_key_set_key({table.get_key(read_id), n_pass});

            pass_key_vec.push_back(pass_key);

            table.record_write(write_id,
                               pass_key);

            std::swap(read_id,
                      write_id);
        }

        return pass_key_vec;
    };

    const auto a_first = run_chain_func(id_a,
                                        id_b);
    const auto b_first = run_chain_func(id_b,
                                        id_a);

    EXPECT_EQ(a_first,
              b_first);

    /* The passes of the chain are different from each other, though. */
    EXPECT_NE(a_first.at(0),
              a_first.at(1) );
    EXPECT_NE(a_first.at(1),
              a_first.at(2) );
}

TEST(ResourceKeyTable, ViewsOfAResourceAreToldApartByLevelAndLayer)
{
    Analysis::APIDumpAnalysis::ResourceKeyTable table;
    const auto                                  id = Analysis::APIDumpAnalysis::make_resource_id(1);

    table.register_resource(id,
                            0x1234);

    const Analysis::APIDumpAnalysis::ResourceView level_0 = {0, 0, id};
    const Analysis::APIDumpAnalysis::ResourceView level_1 = {0, 1, id};
    const Analysis::APIDumpAnalysis::ResourceView layer_1 = {1, 0, id};

    EXPECT_EQ(table.get_view_key(level_0),
              table.get_view_key(level_0) );
    EXPECT_NE(table.get_view_key(level_0),
              table.get_view_key(level_1) );
    EXPECT_NE(table.get_view_key(level_0),
              table.get_view_key(layer_1) );
}

TEST(ResourceKeyTable, AResourceForgottenWithAWriterLeavesNoWriterToTheNextOne)
{
    const auto id = Analysis::APIDumpAnalysis::make_resource_id(2);

    Analysis::APIDumpAnalysis::ResourceKeyTable table;

    table.register_resource(Analysis::APIDumpAnalysis::make_resource_id(1),
                            0x1111);
    table.register_resource(id,
                            0x2222);

    const uint64_t key_before_write = table.get_key(id);

    table.record_write(id,
                       0x9999);
    table.forget_resource(id);

    EXPECT_EQ(table.get_key(id),
              0u);

    table.register_resource(id,
                            0x2222);

    EXPECT_EQ(table.get_key(id),
              key_before_write);

    table.begin_frame();

    EXPECT_EQ(table.get_key(id),
              key_before_write);
    EXPECT_EQ(table.get_unwritten_key(id),
              key_before_write);
}

TEST(ResourceKeyTable, TheResourceNoneAndResourcesPastTheEndHaveNoKeyAndAreNotWrittenTo)
{
    Analysis::APIDumpAnalysis::ResourceKeyTable table;

    table.record_write(Analysis::APIDumpAnalysis::ResourceId::NONE,
                       1);
    table.record_write(Analysis::APIDumpAnalysis::make_resource_id(50),
                       1);
    table.forget_resource(Analysis::APIDumpAnalysis::make_resource_id(50) );
    table.begin_frame();

    EXPECT_EQ(table.get_key(Analysis::APIDumpAnalysis::ResourceId::NONE),
              0u);
    EXPECT_EQ(table.get_key(Analysis::APIDumpAnalysis::make_resource_id(50) ),
              0u);
}

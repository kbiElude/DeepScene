/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for LiveObjectList: the objects come out newest first, and adding one again does not move it.
 */
#include <gtest/gtest.h>
#include "live_object_list.h"

TEST(LiveObjectList, TheObjectsComeOutNewestFirst)
{
    Stereo::Replay::EngineDLL::LiveObjectList list;
    int                                       a = 0;
    int                                       b = 0;
    int                                       c = 0;

    /* The addresses of the objects are not in the order of their creation. */
    list.add(&b);
    list.add(&c);
    list.add(&a);

    EXPECT_EQ(list.get_newest_first_vec(),
              (std::vector<void*>{&a, &c, &b}) );
}

TEST(LiveObjectList, AddingAnObjectAgainLeavesItWhereItIs)
{
    Stereo::Replay::EngineDLL::LiveObjectList list;
    int                                       a = 0;
    int                                       b = 0;

    list.add(&a);
    list.add(&b);
    list.add(&a);

    EXPECT_EQ(list.get_newest_first_vec(),
              (std::vector<void*>{&b, &a}) );
}

TEST(LiveObjectList, ARemovedObjectIsGoneAndOneAddedAfterItIsNewest)
{
    Stereo::Replay::EngineDLL::LiveObjectList list;
    int                                       a = 0;
    int                                       b = 0;

    list.add   (&a);
    list.add   (&b);
    list.remove(&a);
    list.remove(&a);
    list.add   (&a);

    EXPECT_EQ(list.get_newest_first_vec(),
              (std::vector<void*>{&a, &b}) );

    list.clear();

    EXPECT_TRUE(list.get_newest_first_vec().empty() );
}

TEST(LiveObjectList, ManyObjectsAreAddedAndRemovedWithoutAScan)
{
    constexpr size_t N_OBJECTS = 20000;

    Stereo::Replay::EngineDLL::LiveObjectList list;
    std::vector<int>                          object_vec(N_OBJECTS);

    for (auto& object : object_vec)
    {
        list.add(&object);
    }

    for (size_t n_object = 0;
                n_object < N_OBJECTS;
                n_object += 2)
    {
        list.remove(&object_vec.at(n_object) );
    }

    const auto newest_first_vec = list.get_newest_first_vec();

    ASSERT_EQ(newest_first_vec.size(),
              N_OBJECTS / 2);
    EXPECT_EQ(newest_first_vec.front(),
              &object_vec.back() );
    EXPECT_EQ(newest_first_vec.back(),
              &object_vec.at(1) );
}

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the mapping of the handles of a dumped process to the ones of the replay.
 */
#include <gtest/gtest.h>
#include "object_remapper.h"

TEST(ObjectRemapper, ZeroStandsForItself)
{
    Stereo::Replay::EngineDLL::ObjectRemapper remapper;
    uint64_t                                  live_value = 123;

    EXPECT_TRUE(remapper.get(Stereo::Replay::EngineDLL::ObjectKind::HWND,
                             0,
                            &live_value) );
    EXPECT_EQ  (live_value,
                0u);
}

TEST(ObjectRemapper, KindsAreMappedApart)
{
    Stereo::Replay::EngineDLL::ObjectRemapper remapper;
    uint64_t                                  live_value = 0;

    remapper.add(Stereo::Replay::EngineDLL::ObjectKind::HDC,
                 10,
                 20);

    EXPECT_TRUE (remapper.get(Stereo::Replay::EngineDLL::ObjectKind::HDC,
                              10,
                             &live_value) );
    EXPECT_EQ   (live_value,
                 20u);
    EXPECT_FALSE(remapper.get(Stereo::Replay::EngineDLL::ObjectKind::HWND,
                              10,
                             &live_value) );
}

TEST(ObjectRemapper, ARemovedOrClearedValueIsUnknown)
{
    Stereo::Replay::EngineDLL::ObjectRemapper remapper;
    uint64_t                                  live_value = 0;

    remapper.add   (Stereo::Replay::EngineDLL::ObjectKind::HDC,
                    1,
                    2);
    remapper.add   (Stereo::Replay::EngineDLL::ObjectKind::HDC,
                    3,
                    4);
    remapper.remove(Stereo::Replay::EngineDLL::ObjectKind::HDC,
                    1);

    EXPECT_FALSE(remapper.get(Stereo::Replay::EngineDLL::ObjectKind::HDC,
                              1,
                             &live_value) );

    remapper.clear();

    EXPECT_FALSE(remapper.get(Stereo::Replay::EngineDLL::ObjectKind::HDC,
                              3,
                             &live_value) );
}

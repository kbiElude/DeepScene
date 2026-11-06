/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the quit handler of the player: a request from any thread ends the wait, and reaches the target whenever it was set.
 */
#include <gtest/gtest.h>
#include "quit_handler.h"
#include <thread>

TEST(QuitHandlerTest, ARequestFromAnotherThreadEndsTheWait)
{
    Stereo::Replay::Player::QuitHandler handler;

    EXPECT_FALSE(handler.is_requested() );

    std::thread requester([&handler]()
                          {
                              handler.request();
                          });

    handler.wait();
    requester.join();

    EXPECT_TRUE(handler.is_requested() );
}

TEST(QuitHandlerTest, TheTargetIsCalledByEveryRequestAndAtOnceIfOneWasMadeBeforeIt)
{
    Stereo::Replay::Player::QuitHandler handler;
    uint32_t                            n_calls = 0;

    handler.set_target([&n_calls]()
                       {
                           ++n_calls;
                       });

    EXPECT_EQ(n_calls,
              0u);

    handler.request();

    EXPECT_EQ(n_calls,
              1u);

    handler.set_target(nullptr);
    handler.request();

    EXPECT_EQ(n_calls,
              1u);

    handler.set_target([&n_calls]()
                       {
                           ++n_calls;
                       });

    EXPECT_EQ(n_calls,
              2u);
}

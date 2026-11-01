/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Smoke tests of the session of a headset, on a machine which may have no headset, or no runtime: what is there is started, and what is
 * not is said in words.
 */
#include <gtest/gtest.h>
#include "Stereo/StereoXR/headset_session.h"

TEST(HeadsetSessionTest, ItStartsIfThereIsAHeadsetAndSaysWhyItDoesNotIfThereIsNot)
{
    std::string error;
    auto        session_ptr = Stereo::StereoXR::HeadsetSession::create_ptr(&error);

    if (session_ptr == nullptr)
    {
        EXPECT_FALSE(error.empty() );

        GTEST_SKIP() << "No headset: " << error;
    }

    EXPECT_FALSE(session_ptr->get_system_name().empty() );
    EXPECT_NE   (session_ptr->get_device_ptr(),
                 nullptr);
    EXPECT_NE   (session_ptr->get_queue_ptr(),
                 nullptr);
}

TEST(HeadsetSessionTest, ASessionWhichIsAskedToEndEndsAtTheNextFrame)
{
    std::string error;
    auto        session_ptr = Stereo::StereoXR::HeadsetSession::create_ptr(&error);

    if (session_ptr == nullptr)
    {
        GTEST_SKIP() << "No headset: " << error;
    }

    session_ptr->request_exit();

    EXPECT_FALSE(session_ptr->run_frame(&error) );
}

TEST(HeadsetSessionTest, NoTexturesAreGivenBeforeTheSessionRuns)
{
    ID3D12Resource* left_ptr  = nullptr;
    ID3D12Resource* right_ptr = nullptr;
    std::string     error;
    auto            session_ptr = Stereo::StereoXR::HeadsetSession::create_ptr(&error);

    if (session_ptr == nullptr)
    {
        GTEST_SKIP() << "No headset: " << error;
    }

    EXPECT_FALSE(session_ptr->acquire_eye_textures(800,
                                                   600,
                                                  &left_ptr,
                                                  &right_ptr,
                                                  &error) );
    EXPECT_FALSE(error.empty() );
}

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for ExecutorRegistry: an API without an executor is asked about without a crash, and an executor which serves several APIs
 * is listed once.
 */
#include <gtest/gtest.h>
#include "executor_registry.h"
#include "test_doubles.h"

TEST(ExecutorRegistryTest, AnApiWithNoExecutorGetsOneWhichDoesNothing)
{
    const Stereo::Replay::EngineDLL::ExecutorRegistry registry;

    EXPECT_FALSE(registry.has_executor(Capture::APIDumpLoader::APIGroup::UNKNOWN) );
    EXPECT_NE(registry.get_executor_ptr(Capture::APIDumpLoader::APIGroup::UNKNOWN),
              nullptr);
    EXPECT_TRUE(registry.get_executor_ptr_vec().empty() );
}

TEST(ExecutorRegistryTest, AnExecutorWhichServesSeveralApisIsListedOnce)
{
    Stereo::Replay::EngineDLL::ExecutorRegistry registry;
    const auto                                  shared_ptr = std::make_shared<Tests::StubCallExecutor>();
    const auto                                  single_ptr = std::make_shared<Tests::StubCallExecutor>();

    registry.add(Capture::APIDumpLoader::APIGroup::GDI32,
                 shared_ptr);
    registry.add(Capture::APIDumpLoader::APIGroup::D3D8,
                 single_ptr);
    registry.add(Capture::APIDumpLoader::APIGroup::USER32,
                 shared_ptr);

    const std::vector<Stereo::Replay::EngineDLL::ICallExecutor*> expected_vec = {shared_ptr.get(), single_ptr.get()};

    EXPECT_EQ(registry.get_executor_ptr_vec(),
              expected_vec);
    EXPECT_EQ(registry.get_executor_ptr(Capture::APIDumpLoader::APIGroup::USER32),
              shared_ptr.get() );
    EXPECT_TRUE(registry.has_executor(Capture::APIDumpLoader::APIGroup::D3D8) );
    EXPECT_FALSE(registry.has_executor(Capture::APIDumpLoader::APIGroup::UNKNOWN) );
}

TEST(ExecutorRegistryTest, AddingAnExecutorForAnApiReplacesTheOldOne)
{
    Stereo::Replay::EngineDLL::ExecutorRegistry registry;
    const auto                                  new_ptr = std::make_shared<Tests::StubCallExecutor>();
    const auto                                  old_ptr = std::make_shared<Tests::StubCallExecutor>();

    registry.add(Capture::APIDumpLoader::APIGroup::GL,
                 old_ptr);
    registry.add(Capture::APIDumpLoader::APIGroup::GL,
                 new_ptr);

    const std::vector<Stereo::Replay::EngineDLL::ICallExecutor*> expected_vec = {new_ptr.get()};

    EXPECT_EQ(registry.get_executor_ptr_vec(),
              expected_vec);
}

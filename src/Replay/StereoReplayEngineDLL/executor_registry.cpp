/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "executor_registry.h"
#include "null_call_executor.h"
#include <algorithm>

Replay::StereoReplayEngineDLL::ExecutorRegistry::ExecutorRegistry()
    :m_fallback_executor_ptr(std::make_shared<NullCallExecutor>() )
{
    /* Stub */
}

void Replay::StereoReplayEngineDLL::ExecutorRegistry::add(const Capture::APIDumpLoader::APIGroup& in_group,
                                                          const std::shared_ptr<ICallExecutor>&   in_executor_ptr)
{
    m_executor_ptr_array.at(static_cast<size_t>(in_group) ) = in_executor_ptr;

    m_executor_ptr_vec.clear();

    for (const auto& executor_ptr : m_executor_ptr_array)
    {
        if (executor_ptr != nullptr)
        {
            const auto known_iterator = std::find(m_executor_ptr_vec.begin(),
                                                  m_executor_ptr_vec.end  (),
                                                  executor_ptr.get        () );

            if (known_iterator == m_executor_ptr_vec.end() )
            {
                m_executor_ptr_vec.push_back(executor_ptr.get() );
            }
        }
    }
}

Replay::StereoReplayEngineDLL::ICallExecutor* Replay::StereoReplayEngineDLL::ExecutorRegistry::get_executor_ptr(const Capture::APIDumpLoader::APIGroup& in_group) const
{
    const auto& executor_ptr = m_executor_ptr_array.at(static_cast<size_t>(in_group) );

    return (executor_ptr != nullptr) ? executor_ptr.get()
                                     : m_fallback_executor_ptr.get();
}

const std::vector<Replay::StereoReplayEngineDLL::ICallExecutor*>& Replay::StereoReplayEngineDLL::ExecutorRegistry::get_executor_ptr_vec() const
{
    return m_executor_ptr_vec;
}

bool Replay::StereoReplayEngineDLL::ExecutorRegistry::has_executor(const Capture::APIDumpLoader::APIGroup& in_group) const
{
    return (m_executor_ptr_array.at(static_cast<size_t>(in_group) ) != nullptr);
}

/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_EXECUTOR_REGISTRY_H
#define STEREOREPLAYENGINEDLL_EXECUTOR_REGISTRY_H

#include "Capture/APIDumpLoader/api_info.h"
#include "i_call_executor.h"
#include <array>
#include <memory>
#include <vector>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* The executor of each API. One executor may serve several APIs. */
        class ExecutorRegistry
        {
        public:
            ExecutorRegistry();

            /* Replaces the executor of the API, if it has one. */
            void add(const Capture::APIDumpLoader::APIGroup& in_group,
                     const std::shared_ptr<ICallExecutor>&   in_executor_ptr);

            /* The executor of the API, or one which does nothing, if the API has none: for asking about a call, not for executing it. */
            ICallExecutor* get_executor_ptr(const Capture::APIDumpLoader::APIGroup& in_group) const;

            /* Every executor once, in the order of the APIs. Valid until the next add(). */
            const std::vector<ICallExecutor*>& get_executor_ptr_vec() const;

            /* False for an API with no executor. */
            bool has_executor(const Capture::APIDumpLoader::APIGroup& in_group) const;

        private:
            static constexpr size_t N_GROUPS = static_cast<size_t>(Capture::APIDumpLoader::APIGroup::UNKNOWN) + 1;

            std::array<std::shared_ptr<ICallExecutor>, N_GROUPS> m_executor_ptr_array;
            std::vector<ICallExecutor*>                          m_executor_ptr_vec;
            std::shared_ptr<ICallExecutor>                       m_fallback_executor_ptr;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_EXECUTOR_REGISTRY_H */

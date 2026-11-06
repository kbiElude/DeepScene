/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "quit_handler.h"

Stereo::Replay::Player::QuitHandler::QuitHandler()
{
    /* Stub */
}

bool Stereo::Replay::Player::QuitHandler::is_requested() const
{
    std::lock_guard<std::mutex> lock(m_state.mutex);

    return m_state.is_requested;
}

void Stereo::Replay::Player::QuitHandler::request()
{
    std::lock_guard<std::mutex> lock(m_state.mutex);

    m_state.is_requested = true;

    m_state.cv.notify_all();

    if (m_state.target_func != nullptr)
    {
        m_state.target_func();
    }
}

void Stereo::Replay::Player::QuitHandler::set_target(const TargetFunc& in_target_func)
{
    std::lock_guard<std::mutex> lock(m_state.mutex);

    m_state.target_func = in_target_func;

    if (m_state.is_requested            &&
        m_state.target_func  != nullptr)
    {
        m_state.target_func();
    }
}

void Stereo::Replay::Player::QuitHandler::wait()
{
    std::unique_lock<std::mutex> lock(m_state.mutex);

    const auto is_requested_func = [this]()
    {
        return m_state.is_requested;
    };

    m_state.cv.wait(lock,
                    is_requested_func);
}

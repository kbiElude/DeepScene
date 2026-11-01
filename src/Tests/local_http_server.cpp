/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "local_http_server.h"
#include <winsock2.h>
#include <ws2tcpip.h>

Tests::LocalHttpServer::LocalHttpServer()
    :m_port         (0),
     m_should_stop  (false),
     m_socket       (static_cast<uint64_t>(INVALID_SOCKET) )
{
    WSADATA data;

    ::WSAStartup(MAKEWORD(2, 2),
                &data);
}

Tests::LocalHttpServer::~LocalHttpServer()
{
    m_should_stop = true;

    if (m_thread.joinable() )
    {
        m_thread.join();
    }

    if (m_socket != static_cast<uint64_t>(INVALID_SOCKET) )
    {
        ::closesocket(static_cast<SOCKET>(m_socket) );
    }

    ::WSACleanup();
}

std::string Tests::LocalHttpServer::get_url(const std::string& in_path) const
{
    return "http://127.0.0.1:" + std::to_string(m_port) + in_path;
}

void Tests::LocalHttpServer::run()
{
    const SOCKET listening = static_cast<SOCKET>(m_socket);

    while (m_should_stop.load() == false)
    {
        fd_set read_set;
        timeval timeout;

        FD_ZERO(&read_set);
        FD_SET (listening,
               &read_set);

        timeout.tv_sec  = 0;
        timeout.tv_usec = 50000;

        if (::select(0,
                    &read_set,
                     nullptr,
                     nullptr,
                    &timeout) > 0)
        {
            const SOCKET client = ::accept(listening,
                                           nullptr,
                                           nullptr);

            if (client != INVALID_SOCKET)
            {
                char        request_i8vec2048[2048] = {};
                std::string path;
                Answer      answer;
                const int   n_received = ::recv(client,
                                                request_i8vec2048,
                                                sizeof(request_i8vec2048) - 1,
                                                0);

                /* "GET /path HTTP/1.1" */
                if (n_received > 0)
                {
                    const std::string request(request_i8vec2048);
                    const size_t      n_start = request.find(' ');
                    const size_t      n_end   = request.find(' ',
                                                             n_start + 1);

                    path = request.substr(n_start + 1,
                                          n_end - n_start - 1);
                }

                answer.status = 404;

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    const auto                  iterator = m_answer_map.find(path);

                    if (iterator != m_answer_map.end() )
                    {
                        answer = iterator->second;
                    }
                }

                {
                    const uint64_t    n_length = (answer.n_declared_bytes != 0) ? answer.n_declared_bytes
                                                                                : answer.body.size();
                    const std::string head     = "HTTP/1.1 " + std::to_string(answer.status) + " X\r\n" +
                                                 "Content-Length: " + std::to_string(n_length) + "\r\n" +
                                                 ((answer.location.empty() ) ? std::string()
                                                                             : "Location: " + answer.location + "\r\n") +
                                                 "Connection: close\r\n\r\n";
                    const size_t      n_first  = (answer.should_stall) ? answer.body.size() / 2
                                                                       : answer.body.size();

                    ::send(client,
                           head.data(),
                           static_cast<int>(head.size() ),
                           0);

                    if (n_first > 0)
                    {
                        ::send(client,
                               answer.body.data(),
                               static_cast<int>(n_first),
                               0);
                    }

                    while (answer.should_stall && m_should_stop.load() == false)
                    {
                        ::Sleep(20);
                    }
                }

                ::closesocket(client);
            }
        }
    }
}

void Tests::LocalHttpServer::set_answer(const std::string& in_path,
                                        const Answer&      in_answer)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_answer_map[in_path] = in_answer;
}

bool Tests::LocalHttpServer::start()
{
    sockaddr_in address;
    int         n_address_bytes = sizeof(address);
    SOCKET      listening       = ::socket(AF_INET,
                                           SOCK_STREAM,
                                           IPPROTO_TCP);
    bool        result          = false;

    if (listening == INVALID_SOCKET)
    {
        goto end;
    }

    ::ZeroMemory(&address,
                  sizeof(address) );

    address.sin_family = AF_INET;
    address.sin_port   = 0;

    ::inet_pton(AF_INET,
                "127.0.0.1",
               &address.sin_addr);

    if (::bind(listening,
               reinterpret_cast<sockaddr*>(&address),
               sizeof(address) ) != 0)
    {
        ::closesocket(listening);

        goto end;
    }

    if (::listen(listening,
                 4) != 0)
    {
        ::closesocket(listening);

        goto end;
    }

    ::getsockname(listening,
                  reinterpret_cast<sockaddr*>(&address),
                 &n_address_bytes);

    m_port   = ntohs(address.sin_port);
    m_socket = static_cast<uint64_t>(listening);
    m_thread = std::thread([this]() { run(); } );
    result   = true;

end:
    return result;
}

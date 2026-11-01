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
    WSADATA winsock_data;

    ::WSAStartup(MAKEWORD(2, 2),
                &winsock_data);
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
    const SOCKET listening_socket = static_cast<SOCKET>(m_socket);

    while (m_should_stop.load() == false)
    {
        fd_set  read_set;
        timeval select_timeout;

        FD_ZERO(&read_set);
        FD_SET (listening_socket,
               &read_set);

        select_timeout.tv_sec  = 0;
        select_timeout.tv_usec = 50000;

        if (::select(0,
                    &read_set,
                     nullptr,
                     nullptr,
                    &select_timeout) > 0)
        {
            const SOCKET client_socket = ::accept(listening_socket,
                                                  nullptr,
                                                  nullptr);

            if (client_socket != INVALID_SOCKET)
            {
                std::string request_path;
                char        request_text_i8vec2048[2048] = {};
                Answer      served_answer;

                const int n_request_bytes = ::recv(client_socket,
                                                   request_text_i8vec2048,
                                                   sizeof(request_text_i8vec2048) - 1,
                                                   0);

                /* "GET /path HTTP/1.1" */
                if (n_request_bytes > 0)
                {
                    const std::string request_text(request_text_i8vec2048);

                    const size_t n_path_start_character = request_text.find(' ');

                    const size_t n_path_end_character = request_text.find(' ',
                                                                          n_path_start_character + 1);

                    request_path = request_text.substr(n_path_start_character + 1,
                                                       n_path_end_character - n_path_start_character - 1);
                }

                served_answer.http_status_code = 404;

                {
                    std::lock_guard<std::mutex> answers_lock(m_mutex);

                    const auto answer_iterator = m_answer_map.find(request_path);

                    if (answer_iterator != m_answer_map.end() )
                    {
                        served_answer = answer_iterator->second;
                    }
                }

                {
                    const uint64_t    n_content_length_bytes = (served_answer.n_declared_bytes != 0) ? served_answer.n_declared_bytes
                                                                                                     : served_answer.response_body.size();
                    const std::string response_header_text   = "HTTP/1.1 " + std::to_string(served_answer.http_status_code) + " X\r\n" +
                                                               "Content-Length: " + std::to_string(n_content_length_bytes) + "\r\n" +
                                                               ((served_answer.redirect_location.empty() ) ? std::string()
                                                                                                           : "Location: " + served_answer.redirect_location + "\r\n") +
                                                               "Connection: close\r\n\r\n";
                    const size_t n_body_bytes_sent_first = (served_answer.should_stall) ? served_answer.response_body.size() / 2
                                                                                        : served_answer.response_body.size();

                    ::send(client_socket,
                           response_header_text.data(),
                           static_cast<int>(response_header_text.size() ),
                           0);

                    if (n_body_bytes_sent_first > 0)
                    {
                        ::send(client_socket,
                               served_answer.response_body.data(),
                               static_cast<int>(n_body_bytes_sent_first),
                               0);
                    }

                    while (served_answer.should_stall && m_should_stop.load() == false)
                    {
                        ::Sleep(20);
                    }
                }

                ::closesocket(client_socket);
            }
        }
    }
}

void Tests::LocalHttpServer::set_answer(const std::string& in_path,
                                        const Answer&      in_answer)
{
    std::lock_guard<std::mutex> answers_lock(m_mutex);

    m_answer_map[in_path] = in_answer;
}

bool Tests::LocalHttpServer::start()
{
    sockaddr_in listening_address;
    SOCKET      listening_socket = ::socket(AF_INET,
                                            SOCK_STREAM,
                                            IPPROTO_TCP);
    int         n_address_bytes  = sizeof(listening_address);
    bool        result           = false;

    const auto run_func = [this]()
    {
        run();
    };

    if (listening_socket == INVALID_SOCKET)
    {
        goto end;
    }

    ::ZeroMemory(&listening_address,
                  sizeof(listening_address) );

    listening_address.sin_family = AF_INET;
    listening_address.sin_port   = 0;

    ::inet_pton(AF_INET,
                "127.0.0.1",
               &listening_address.sin_addr);

    if (::bind(listening_socket,
               reinterpret_cast<sockaddr*>(&listening_address),
               sizeof(listening_address) ) != 0)
    {
        ::closesocket(listening_socket);

        goto end;
    }

    if (::listen(listening_socket,
                 4) != 0)
    {
        ::closesocket(listening_socket);

        goto end;
    }

    ::getsockname(listening_socket,
                  reinterpret_cast<sockaddr*>(&listening_address),
                 &n_address_bytes);

    m_port   = ntohs(listening_address.sin_port);
    m_socket = static_cast<uint64_t>(listening_socket);
    m_thread = std::thread(run_func);
    result   = true;

end:
    return result;
}

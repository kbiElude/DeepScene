/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef TESTS_LOCAL_HTTP_SERVER_H
#define TESTS_LOCAL_HTTP_SERVER_H

#include <atomic>
#include <map>
#include <mutex>
#include <stdint.h>
#include <string>
#include <thread>

namespace Tests
{
    /* A server of HTTP on this computer (127.0.0.1, a port which the system gives), for the tests of what downloads. It answers
     * one request at a time, from the answers which the test has given for the paths.
     */
    class LocalHttpServer
    {
    public:
        struct Answer
        {
            int         http_status_code = 200;
            uint64_t    n_declared_bytes = 0;     /* the length which is said, if it is not the one of the body (0: the real one) */
            std::string redirect_location;        /* of a redirect, if there is one                                               */
            std::string response_body;
            bool        should_stall     = false; /* half of the body is sent, and then nothing until the server is stopped       */
        };

        LocalHttpServer();
        ~LocalHttpServer();

        LocalHttpServer(const LocalHttpServer&)            = delete;
        LocalHttpServer& operator=(const LocalHttpServer&) = delete;

        /* The address of a path, for example "http://127.0.0.1:50321/file". */
        std::string get_url(const std::string& in_path) const;

        void set_answer(const std::string& in_path,
                        const Answer&      in_answer);

        /* False if it cannot listen. */
        bool start();

    private:
        void run();

        std::map<std::string, Answer> m_answer_map;   /* guarded by m_mutex */
        std::mutex                    m_mutex;
        int                           m_port;
        std::atomic<bool>             m_should_stop;
        uint64_t                      m_socket;       /* SOCKET of the listening */
        std::thread                   m_thread;
    };
}

#endif /* TESTS_LOCAL_HTTP_SERVER_H */

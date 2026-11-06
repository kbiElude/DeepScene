/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef TESTS_TEMP_DIRECTORY_H
#define TESTS_TEMP_DIRECTORY_H

#include <string>

namespace Tests
{
    /* A directory of its own in the temporary directory of the system, which is removed, with what is in it, when the object goes. */
    class TempDirectory
    {
    public:
        TempDirectory();
        ~TempDirectory();

        TempDirectory(const TempDirectory&)            = delete;
        TempDirectory& operator=(const TempDirectory&) = delete;

        const std::string& get_path() const;

    private:
        std::string m_path;
    };
}

#endif /* TESTS_TEMP_DIRECTORY_H */

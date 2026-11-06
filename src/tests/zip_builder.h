/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef TESTS_ZIP_BUILDER_H
#define TESTS_ZIP_BUILDER_H

#include <string>
#include <utility>
#include <vector>

namespace Tests
{
    /* A zip file for a test, made with miniz: the names are what is given, so a name which an unpacker must not accept can be made. */
    class ZipBuilder
    {
    public:
        /* An entry. A name which ends with '/' is a directory. */
        void add_file(const std::string& in_name,
                      const std::string& in_content);

        /* The zip file, as bytes; empty if it could not be made. */
        std::string get_bytes() const;

        bool write_file(const std::string& in_path) const;

    private:
        std::vector<std::pair<std::string, std::string> > m_entry_vec;
    };
}

#endif /* TESTS_ZIP_BUILDER_H */

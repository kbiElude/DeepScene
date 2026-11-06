/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads JSON text (RFC 8259) into a tree of values. It is strict: no trailing commas, comments, leading zeros, raw control
 * characters in strings, repeated keys or numbers that overflow a double. Nesting deeper than 64 levels is refused, so that a
 * damaged or hostile file cannot overflow the stack.
 */
#ifndef UTILS_JSON_JSON_READER_H
#define UTILS_JSON_JSON_READER_H

#include "utils/json/json_value.h"
#include <stddef.h>
#include <stdint.h>
#include <string>

namespace Utils
{
    namespace Json
    {
        class JsonReader
        {
        public:
            /* Returns false if the text is not exactly one JSON value. The error, if asked for, says where and why. */
            static bool parse(const std::string& in_text,
                              JsonValue*         out_value_ptr,
                              std::string*       out_error_ptr);

            /* Returns false if the file cannot be opened. */
            static bool read_file(const std::string& in_filename,
                                  std::string*       out_text_ptr);

            /* Some editors, and PowerShell 5.1, start a UTF-8 file with a byte order mark; JSON does not allow it, so a reader of files
             * takes it off before parse().
             */
            static std::string strip_byte_order_mark(const std::string& in_text);

        private:
            JsonReader(const std::string& in_text);

            bool fail(const std::string& in_message);

            bool parse_array         (const size_t& in_depth,
                                      JsonValue*    out_value_ptr);
            bool parse_literal       (const char*   in_literal_ptr);
            bool parse_number        (JsonValue*    out_value_ptr);
            bool parse_object        (const size_t& in_depth,
                                      JsonValue*    out_value_ptr);
            bool parse_string        (std::string*  out_string_ptr);
            bool parse_unicode_escape(std::string*  inout_string_ptr);
            bool parse_value         (const size_t& in_depth,
                                      JsonValue*    out_value_ptr);

            bool read_hex_digits(uint32_t* out_value_ptr);

            void skip_white_space();

            static void append_utf8(const uint32_t& in_code_point,
                                    std::string*    inout_string_ptr);

            std::string        m_error;
            size_t             m_position;
            const std::string& m_text;
        };
    }
}

#endif /* UTILS_JSON_JSON_READER_H */

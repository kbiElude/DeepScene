/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes JSON text. The output is deterministic: keys appear in the order written, and numbers are always formatted
 * the same way. Misuse (a value without a key in an object, a key in an array, mismatched ends, a second top-level
 * value) sets has_failed(), and the text is then unusable.
 */
#ifndef APIDUMPANALYSIS_JSON_WRITER_H
#define APIDUMPANALYSIS_JSON_WRITER_H

#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class JsonWriter
        {
        public:
            /* Pretty text has one item per line, indented; otherwise everything is on one line. */
            explicit JsonWriter(const bool& in_is_pretty);

            void begin_array();

            /* An array on one line, also in pretty text (for vectors of numbers). Scalars only. */
            void begin_inline_array();

            void begin_object();
            void end_array   ();
            void end_object  ();

            /* True if the text is not one complete JSON value: something was misused, or a container is still open. */
            bool has_failed() const;

            const std::string& get_text() const;

            void write_bool(const bool& in_value);

            /* NaN and infinity are not JSON; they are written as null. */
            void write_double(const double& in_value);

            /* Like write_double, with the digits of a float instead of those of the double it widens to. */
            void write_float(const float& in_value);

            /* As the string "0x" and 16 hex digits: JSON numbers cannot hold 64 bits exactly. */
            void write_hex_u64(const uint64_t& in_value);

            void write_i64   (const int64_t&     in_value);
            void write_key   (const std::string& in_key);
            void write_null  ();
            void write_string(const std::string& in_value);
            void write_u64   (const uint64_t&    in_value);

        private:
            struct Level
            {
                bool   has_key   = false; /* an object key was written and its value is pending */
                bool   is_inline = false;
                bool   is_object = false;
                size_t n_items   = 0;
            };

            void begin_container(const bool& in_is_object,
                                 const bool& in_is_inline);

            /* Writes what precedes a value (separator, indentation). Fails the writer, and returns false, if no value is allowed
             * here.
             */
            bool begin_value();

            void end_container(const bool& in_is_object);

            void write_indentation   (const size_t&      in_depth);
            void write_raw           (const char*        in_text_ptr);
            void write_string_literal(const std::string& in_value);

            bool               m_has_failed;
            bool               m_has_top_level_value;
            bool               m_is_pretty;
            std::vector<Level> m_level_vec;
            std::string        m_text;
        };
    }
}

#endif /* APIDUMPANALYSIS_JSON_WRITER_H */

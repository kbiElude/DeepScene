/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/json_writer.h"
#include <math.h>
#include <stdio.h>

Analysis::APIDumpAnalysis::JsonWriter::JsonWriter(const bool& in_is_pretty)
    :m_has_failed         (false),
     m_has_top_level_value(false),
     m_is_pretty          (in_is_pretty)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::JsonWriter::begin_array()
{
    begin_container(false,  /* in_is_object */
                    false); /* in_is_inline */
}

void Analysis::APIDumpAnalysis::JsonWriter::begin_container(const bool& in_is_object,
                                                            const bool& in_is_inline)
{
    const bool is_in_inline_array = (m_level_vec.empty()          == false &&
                                     m_level_vec.back().is_inline);

    if (is_in_inline_array)
    {
        m_has_failed = true;
    }
    else
    if (begin_value() )
    {
        Level level;

        level.is_inline = in_is_inline;
        level.is_object = in_is_object;

        m_level_vec.push_back(level);

        write_raw((in_is_object) ? "{"
                                 : "[");
    }
}

void Analysis::APIDumpAnalysis::JsonWriter::begin_inline_array()
{
    begin_container(false, /* in_is_object */
                    true); /* in_is_inline */
}

void Analysis::APIDumpAnalysis::JsonWriter::begin_object()
{
    begin_container(true,   /* in_is_object */
                    false); /* in_is_inline */
}

bool Analysis::APIDumpAnalysis::JsonWriter::begin_value()
{
    bool result = false;

    if (m_level_vec.empty() )
    {
        result = (m_has_top_level_value == false);

        m_has_top_level_value = true;
    }
    else
    if (m_level_vec.back().is_object)
    {
        result = m_level_vec.back().has_key;

        m_level_vec.back().has_key = false;
    }
    else
    {
        if (m_level_vec.back().n_items > 0)
        {
            write_raw((m_level_vec.back().is_inline && m_is_pretty) ? ", "
                                                                    : ",");
        }

        if (m_level_vec.back().is_inline == false)
        {
            write_indentation(m_level_vec.size() );
        }

        m_level_vec.back().n_items++;

        result = true;
    }

    if (result == false)
    {
        m_has_failed = true;
    }

    return result;
}

void Analysis::APIDumpAnalysis::JsonWriter::end_array()
{
    end_container(false); /* in_is_object */
}

void Analysis::APIDumpAnalysis::JsonWriter::end_container(const bool& in_is_object)
{
    if (m_level_vec.empty()                           ||
        m_level_vec.back ().is_object != in_is_object ||
        m_level_vec.back ().has_key)
    {
        m_has_failed = true;
    }
    else
    {
        const bool has_items = (m_level_vec.back().n_items   >  0     &&
                                m_level_vec.back().is_inline == false);

        m_level_vec.pop_back();

        if (has_items)
        {
            write_indentation(m_level_vec.size() );
        }

        write_raw((in_is_object) ? "}"
                                 : "]");
    }
}

void Analysis::APIDumpAnalysis::JsonWriter::end_object()
{
    end_container(true); /* in_is_object */
}

const std::string& Analysis::APIDumpAnalysis::JsonWriter::get_text() const
{
    return m_text;
}

bool Analysis::APIDumpAnalysis::JsonWriter::has_failed() const
{
    return (m_has_failed                   ||
            m_level_vec.empty()   == false ||
            m_has_top_level_value == false);
}

void Analysis::APIDumpAnalysis::JsonWriter::write_bool(const bool& in_value)
{
    if (begin_value() )
    {
        write_raw((in_value) ? "true"
                             : "false");
    }
}

void Analysis::APIDumpAnalysis::JsonWriter::write_double(const double& in_value)
{
    constexpr size_t N_BUFFER_BYTES = 32;

    char buffer[N_BUFFER_BYTES];

    if (begin_value() )
    {
        if (::isfinite(in_value) )
        {
            ::snprintf(buffer,
                       sizeof(buffer),
                       "%.9g",
                       in_value);

            write_raw(buffer);
        }
        else
        {
            write_raw("null");
        }
    }
}

void Analysis::APIDumpAnalysis::JsonWriter::write_float(const float& in_value)
{
    constexpr size_t N_BUFFER_BYTES = 32;
    constexpr int    N_DIGITS       = 7;

    char buffer[N_BUFFER_BYTES];

    if (begin_value() )
    {
        if (::isfinite(in_value) )
        {
            ::snprintf(buffer,
                       sizeof(buffer),
                       "%.*g",
                       N_DIGITS,
                       static_cast<double>(in_value) );

            write_raw(buffer);
        }
        else
        {
            write_raw("null");
        }
    }
}

void Analysis::APIDumpAnalysis::JsonWriter::write_hex_u64(const uint64_t& in_value)
{
    constexpr size_t N_BUFFER_BYTES = 32;

    char buffer[N_BUFFER_BYTES];

    if (begin_value() )
    {
        ::snprintf(buffer,
                   sizeof(buffer),
                   "\"0x%016llx\"",
                   static_cast<unsigned long long>(in_value) );

        write_raw(buffer);
    }
}

void Analysis::APIDumpAnalysis::JsonWriter::write_i64(const int64_t& in_value)
{
    constexpr size_t N_BUFFER_BYTES = 32;

    char buffer[N_BUFFER_BYTES];

    if (begin_value() )
    {
        ::snprintf(buffer,
                   sizeof(buffer),
                   "%lld",
                   static_cast<long long>(in_value) );

        write_raw(buffer);
    }
}

void Analysis::APIDumpAnalysis::JsonWriter::write_indentation(const size_t& in_depth)
{
    constexpr size_t N_SPACES_PER_LEVEL = 2;

    if (m_is_pretty)
    {
        m_text.push_back('\n');
        m_text.append   (in_depth * N_SPACES_PER_LEVEL,
                         ' ');
    }
}

void Analysis::APIDumpAnalysis::JsonWriter::write_key(const std::string& in_key)
{
    if (m_level_vec.empty()                    ||
        m_level_vec.back ().is_object == false ||
        m_level_vec.back ().has_key)
    {
        m_has_failed = true;
    }
    else
    {
        if (m_level_vec.back().n_items > 0)
        {
            write_raw(",");
        }

        write_indentation(m_level_vec.size() );

        m_level_vec.back().has_key = true;
        m_level_vec.back().n_items++;

        write_string_literal(in_key);

        write_raw((m_is_pretty) ? ": "
                                : ":");
    }
}

void Analysis::APIDumpAnalysis::JsonWriter::write_null()
{
    if (begin_value() )
    {
        write_raw("null");
    }
}

void Analysis::APIDumpAnalysis::JsonWriter::write_raw(const char* in_text_ptr)
{
    m_text.append(in_text_ptr);
}

void Analysis::APIDumpAnalysis::JsonWriter::write_string(const std::string& in_value)
{
    if (begin_value() )
    {
        write_string_literal(in_value);
    }
}

void Analysis::APIDumpAnalysis::JsonWriter::write_string_literal(const std::string& in_value)
{
    /* Characters with a short escape. */
    struct Escape
    {
        char        character;
        const char* text_ptr;
    };

    static const Escape ESCAPE_ARRAY[] =
    {
        {'"',  "\\\""},
        {'\\', "\\\\"},
        {'\b', "\\b"},
        {'\f', "\\f"},
        {'\n', "\\n"},
        {'\r', "\\r"},
        {'\t', "\\t"}
    };

    constexpr unsigned char FIRST_PRINTABLE_CHARACTER = 0x20; /* characters below this are control characters */
    constexpr size_t        N_BUFFER_BYTES            = 8;

    size_t run_start = 0; /* where the characters which need no escape begin: they are appended all at once */

    write_raw("\"");

    for (size_t n_character = 0;
                n_character < in_value.size();
              ++n_character)
    {
        char        buffer[N_BUFFER_BYTES];
        const char  character  = in_value.at(n_character);
        const char* escape_ptr = nullptr;

        for (const auto& escape : ESCAPE_ARRAY)
        {
            if (escape.character == character)
            {
                escape_ptr = escape.text_ptr;
            }
        }

        /* Control characters without a short escape use \u00XX; UTF-8 bytes pass through unchanged. */
        if (escape_ptr                            == nullptr                   &&
            static_cast<unsigned char>(character) <  FIRST_PRINTABLE_CHARACTER)
        {
            ::snprintf(buffer,
                       sizeof(buffer),
                       "\\u%04x",
                       static_cast<unsigned int>(static_cast<unsigned char>(character) ));

            escape_ptr = buffer;
        }

        if (escape_ptr != nullptr)
        {
            m_text.append(in_value,
                          run_start,
                          n_character - run_start);

            write_raw(escape_ptr);

            run_start = n_character + 1;
        }
    }

    m_text.append(in_value,
                  run_start,
                  in_value.size() - run_start);

    write_raw("\"");
}

void Analysis::APIDumpAnalysis::JsonWriter::write_u64(const uint64_t& in_value)
{
    constexpr size_t N_BUFFER_BYTES = 32;

    char buffer[N_BUFFER_BYTES];

    if (begin_value() )
    {
        ::snprintf(buffer,
                   sizeof(buffer),
                   "%llu",
                   static_cast<unsigned long long>(in_value) );

        write_raw(buffer);
    }
}

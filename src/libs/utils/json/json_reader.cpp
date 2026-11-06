/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/json/json_reader.h"
#include <ctype.h>
#include <fstream>
#include <math.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <unordered_set>
#include <utility>

Utils::Json::JsonReader::JsonReader(const std::string& in_text)
    :m_position(0),
     m_text    (in_text)
{
    /* Stub */
}

bool Utils::Json::JsonReader::fail(const std::string& in_message)
{
    size_t column = 1;
    size_t line   = 1;

    if (m_error.empty() )
    {
        for (size_t n_character = 0;
                    n_character < m_position && n_character < m_text.size();
                  ++n_character)
        {
            if (m_text.at(n_character) == '\n')
            {
                ++line;

                column = 1;
            }
            else
            {
                ++column;
            }
        }

        m_error = "line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + in_message;
    }

    return false;
}

bool Utils::Json::JsonReader::parse(const std::string& in_text,
                                    JsonValue*         out_value_ptr,
                                    std::string*       out_error_ptr)
{
    JsonReader reader(in_text);
    bool       result = false;

    *out_value_ptr = JsonValue();

    if (reader.parse_value(0, /* in_depth */
                           out_value_ptr) )
    {
        reader.skip_white_space();

        if (reader.m_position == in_text.size() )
        {
            result = true;
        }
        else
        {
            reader.fail("unexpected text after the value");
        }
    }

    if (result == false &&
        out_error_ptr != nullptr)
    {
        *out_error_ptr = reader.m_error;
    }

    return result;
}

bool Utils::Json::JsonReader::parse_array(const size_t& in_depth,
                                          JsonValue*    out_value_ptr)
{
    bool result = false;

    out_value_ptr->type = JsonValue::Type::ARRAY;

    ++m_position; /* [ */

    skip_white_space();

    if (m_position            <  m_text.size() &&
        m_text.at(m_position) == ']')
    {
        ++m_position;

        result = true;
    }
    else
    {
        while (true)
        {
            JsonValue item;

            if (parse_value(in_depth + 1,
                           &item) == false)
            {
                break;
            }

            out_value_ptr->item_vec.push_back(std::move(item) );

            skip_white_space();

            if (m_position            <  m_text.size() &&
                m_text.at(m_position) == ',')
            {
                ++m_position;
            }
            else
            if (m_position            <  m_text.size() &&
                m_text.at(m_position) == ']')
            {
                ++m_position;

                result = true;

                break;
            }
            else
            {
                fail("expected ',' or ']'");

                break;
            }
        }
    }

    return result;
}

bool Utils::Json::JsonReader::parse_literal(const char* in_literal_ptr)
{
    const size_t n_characters = ::strlen(in_literal_ptr);
    const bool   result       = (m_text.compare(m_position,
                                                n_characters,
                                                in_literal_ptr) == 0);

    if (result)
    {
        m_position += n_characters;
    }
    else
    {
        fail(std::string("expected ") + in_literal_ptr);
    }

    return result;
}

bool Utils::Json::JsonReader::parse_number(JsonValue* out_value_ptr)
{
    constexpr size_t N_BUFFER_BYTES = 64;

    const auto is_digit_func = [&]()
    {
        return (m_position < m_text.size() &&
                ::isdigit(static_cast<unsigned char>(m_text.at(m_position) )) != 0);
    };

    const size_t start  = m_position;
    bool         result = false;

    out_value_ptr->type = JsonValue::Type::NUMBER;

    if (m_position            <  m_text.size() &&
        m_text.at(m_position) == '-')
    {
        ++m_position;
    }

    if (is_digit_func() == false)
    {
        fail("expected a number");

        goto end;
    }

    /* JSON allows no leading zeros. */
    if (m_text.at(m_position) == '0')
    {
        ++m_position;
    }
    else
    {
        while (is_digit_func() )
        {
            ++m_position;
        }
    }

    if (m_position            <  m_text.size() &&
        m_text.at(m_position) == '.')
    {
        ++m_position;

        if (is_digit_func() == false)
        {
            fail("expected digits after '.'");

            goto end;
        }

        while (is_digit_func() )
        {
            ++m_position;
        }
    }

    if (m_position             <  m_text.size() &&
        (m_text.at(m_position) == 'e'           ||
         m_text.at(m_position) == 'E'))
    {
        ++m_position;

        if (m_position             <  m_text.size() &&
            (m_text.at(m_position) == '+'           ||
             m_text.at(m_position) == '-'))
        {
            ++m_position;
        }

        if (is_digit_func() == false)
        {
            fail("expected digits in the exponent");

            goto end;
        }

        while (is_digit_func() )
        {
            ++m_position;
        }
    }

    /* strtod wants text which ends: a short number is copied to the stack for it, not to a string. */
    {
        char         buffer[N_BUFFER_BYTES];
        const size_t n_chars = m_position - start;

        if (n_chars < N_BUFFER_BYTES)
        {
            ::memcpy(buffer,
                     m_text.data() + start,
                     n_chars);

            buffer[n_chars] = '\0';

            out_value_ptr->number_value = ::strtod(buffer,
                                                   nullptr);
        }
        else
        {
            const std::string number_text = m_text.substr(start,
                                                          n_chars);

            out_value_ptr->number_value = ::strtod(number_text.c_str(),
                                                   nullptr);
        }
    }

    if (::isfinite(out_value_ptr->number_value) == 0)
    {
        fail("the number does not fit a double");

        goto end;
    }

    result = true;

end:
    return result;
}

bool Utils::Json::JsonReader::parse_object(const size_t& in_depth,
                                           JsonValue*    out_value_ptr)
{
    bool result = false;

    out_value_ptr->type = JsonValue::Type::OBJECT;

    ++m_position; /* { */

    skip_white_space();

    if (m_position            <  m_text.size() &&
        m_text.at(m_position) == '}')
    {
        ++m_position;

        result = true;
    }
    else
    {
        std::unordered_set<std::string> key_set; /* the keys so far: a search of the object for each key would be quadratic */

        while (true)
        {
            JsonValue   item;
            std::string key;

            skip_white_space();

            if (m_position            >= m_text.size() ||
                m_text.at(m_position) != '"')
            {
                fail("expected a key");

                break;
            }

            if (parse_string(&key) == false)
            {
                break;
            }

            if (key_set.insert(key).second == false)
            {
                fail("the key \"" + key + "\" is repeated");

                break;
            }

            skip_white_space();

            if (m_position            >= m_text.size() ||
                m_text.at(m_position) != ':')
            {
                fail("expected ':'");

                break;
            }

            ++m_position;

            if (parse_value(in_depth + 1,
                           &item) == false)
            {
                break;
            }

            out_value_ptr->item_vec.push_back(std::move(item) );
            out_value_ptr->key_vec.push_back (key);

            skip_white_space();

            if (m_position            <  m_text.size() &&
                m_text.at(m_position) == ',')
            {
                ++m_position;
            }
            else
            if (m_position            <  m_text.size() &&
                m_text.at(m_position) == '}')
            {
                ++m_position;

                result = true;

                break;
            }
            else
            {
                fail("expected ',' or '}'");

                break;
            }
        }
    }

    return result;
}

bool Utils::Json::JsonReader::parse_string(std::string* out_string_ptr)
{
    constexpr unsigned char FIRST_PRINTABLE_CHARACTER = 0x20; /* characters below this must be escaped */

    bool result = false;

    out_string_ptr->clear();

    ++m_position; /* the opening quote */

    while (m_position < m_text.size() )
    {
        const char character = m_text.at(m_position);

        if (character == '"')
        {
            ++m_position;

            result = true;

            break;
        }

        if (static_cast<unsigned char>(character) < FIRST_PRINTABLE_CHARACTER)
        {
            fail("a control character in a string must be escaped");

            break;
        }

        if (character != '\\')
        {
            /* The characters up to the next quote, backslash or control character are taken together. */
            size_t run_end = m_position + 1;

            while (run_end                                        <  m_text.size()             &&
                   m_text.at(run_end)                             != '"'                       &&
                   m_text.at(run_end)                             != '\\'                      &&
                   static_cast<unsigned char>(m_text.at(run_end)) >= FIRST_PRINTABLE_CHARACTER)
            {
                ++run_end;
            }

            out_string_ptr->append(m_text,
                                   m_position,
                                   run_end - m_position);

            m_position = run_end;

            continue;
        }

        ++m_position; /* the backslash */

        if (m_position >= m_text.size() )
        {
            break;
        }

        const char escaped = m_text.at(m_position);

        if (escaped == 'u')
        {
            ++m_position;

            if (parse_unicode_escape(out_string_ptr) == false)
            {
                break;
            }
        }
        else
        {
            bool is_valid_escape = true;

            switch (escaped)
            {
                case 'b':
                {
                    out_string_ptr->push_back('\b');

                    break;
                }

                case 'f':
                {
                    out_string_ptr->push_back('\f');

                    break;
                }

                case 'n':
                {
                    out_string_ptr->push_back('\n');

                    break;
                }

                case 'r':
                {
                    out_string_ptr->push_back('\r');

                    break;
                }

                case 't':
                {
                    out_string_ptr->push_back('\t');

                    break;
                }

                case '"':
                case '/':
                case '\\':
                {
                    out_string_ptr->push_back(escaped);

                    break;
                }

                default:
                {
                    fail("unknown escape");

                    is_valid_escape = false;

                    break;
                }
            }

            if (is_valid_escape == false)
            {
                break;
            }

            ++m_position;
        }
    }

    if (result == false)
    {
        fail("the string is not closed");
    }

    return result;
}

bool Utils::Json::JsonReader::parse_unicode_escape(std::string* inout_string_ptr)
{
    constexpr uint32_t FIRST_AFTER_SURROGATE    = 0xe000;
    constexpr uint32_t FIRST_HIGH_SURROGATE     = 0xd800;
    constexpr uint32_t FIRST_LOW_SURROGATE      = 0xdc00;
    constexpr uint32_t FIRST_SUPPLEMENTARY      = 0x10000;
    constexpr uint32_t N_SURROGATE_PAYLOAD_BITS = 10;

    uint32_t code_point = 0;
    bool     result     = false;

    if (read_hex_digits(&code_point) == false)
    {
        goto end;
    }

    if (code_point >= FIRST_LOW_SURROGATE   &&
        code_point <  FIRST_AFTER_SURROGATE)
    {
        fail("a low surrogate comes without a high one");

        goto end;
    }

    if (code_point >= FIRST_HIGH_SURROGATE &&
        code_point <  FIRST_LOW_SURROGATE)
    {
        uint32_t low = 0;

        if (m_text.compare(m_position,
                           2, /* n_characters */
                           "\\u") != 0)
        {
            fail("a high surrogate must be followed by a low one");

            goto end;
        }

        m_position += 2;

        if (read_hex_digits(&low) == false)
        {
            goto end;
        }

        if (low <  FIRST_LOW_SURROGATE   ||
            low >= FIRST_AFTER_SURROGATE)
        {
            fail("a high surrogate must be followed by a low one");

            goto end;
        }

        code_point = FIRST_SUPPLEMENTARY + (((code_point - FIRST_HIGH_SURROGATE) << N_SURROGATE_PAYLOAD_BITS) |
                                             (low        - FIRST_LOW_SURROGATE) );
    }

    append_utf8(code_point,
                inout_string_ptr);

    result = true;

end:
    return result;
}

bool Utils::Json::JsonReader::parse_value(const size_t& in_depth,
                                          JsonValue*    out_value_ptr)
{
    constexpr size_t MAX_DEPTH = 64; /* levels of nesting */
    bool             result    = false;

    skip_white_space();

    if (in_depth >= MAX_DEPTH)
    {
        fail("the text is nested too deeply");
    }
    else
    if (m_position >= m_text.size() )
    {
        fail("expected a value");
    }
    else
    {
        const char character = m_text.at(m_position);
        const bool is_digit  = (::isdigit(static_cast<unsigned char>(character) ) != 0);

        if (character == '{')
        {
            result = parse_object(in_depth,
                                  out_value_ptr);
        }
        else
        if (character == '[')
        {
            result = parse_array(in_depth,
                                 out_value_ptr);
        }
        else
        if (character == '"')
        {
            out_value_ptr->type = JsonValue::Type::STRING;

            result = parse_string(&out_value_ptr->string_value);
        }
        else
        if (character == 't')
        {
            out_value_ptr->bool_value = true;
            out_value_ptr->type       = JsonValue::Type::BOOL;

            result = parse_literal("true");
        }
        else
        if (character == 'f')
        {
            out_value_ptr->type = JsonValue::Type::BOOL;

            result = parse_literal("false");
        }
        else
        if (character == 'n')
        {
            out_value_ptr->type = JsonValue::Type::NULL_VALUE;

            result = parse_literal("null");
        }
        else
        if (character == '-' ||
            is_digit)
        {
            result = parse_number(out_value_ptr);
        }
        else
        {
            fail("expected a value");
        }
    }

    return result;
}

bool Utils::Json::JsonReader::read_file(const std::string& in_filename,
                                        std::string*       out_text_ptr)
{
    std::ifstream file(in_filename,
                       std::ios::binary);
    bool          result = false;

    if (file.is_open() )
    {
        std::ostringstream stream;

        stream << file.rdbuf();

        *out_text_ptr = stream.str();
        result        = true;
    }

    return result;
}

bool Utils::Json::JsonReader::read_hex_digits(uint32_t* out_value_ptr)
{
    constexpr size_t   N_BITS_PER_DIGIT          = 4;
    constexpr uint32_t N_FIRST_LETTER_DIGIT_VALUE = 10; /* value of the digit 'a' */
    constexpr size_t   N_HEX_DIGITS              = 4;

    bool result = true;

    *out_value_ptr = 0;

    for (size_t n_digit = 0;
                n_digit < N_HEX_DIGITS;
              ++n_digit)
    {
        uint32_t digit        = 0;
        bool     is_hex_digit = false;

        if (m_position < m_text.size() )
        {
            is_hex_digit = (::isxdigit(static_cast<unsigned char>(m_text.at(m_position) )) != 0);
        }

        if (is_hex_digit == false)
        {
            fail("expected 4 hex digits after \\u");

            result = false;

            break;
        }

        digit = (::isdigit(static_cast<unsigned char>(m_text.at(m_position) )) != 0) ? static_cast<uint32_t>(m_text.at(m_position) - '0')
                                                                                     : static_cast<uint32_t>(::tolower(m_text.at(m_position) ) - 'a' + N_FIRST_LETTER_DIGIT_VALUE);

        *out_value_ptr = (*out_value_ptr << N_BITS_PER_DIGIT) | digit;

        ++m_position;
    }

    return result;
}

std::string Utils::Json::JsonReader::strip_byte_order_mark(const std::string& in_text)
{
    const std::string byte_order_mark("\xEF\xBB\xBF");

    return (in_text.compare(0,
                            byte_order_mark.size(),
                            byte_order_mark) == 0) ? in_text.substr(byte_order_mark.size() )
                                                   : in_text;
}

void Utils::Json::JsonReader::skip_white_space()
{
    while (m_position             <  m_text.size() &&
           (m_text.at(m_position) == ' '           ||
            m_text.at(m_position) == '\n'          ||
            m_text.at(m_position) == '\r'          ||
            m_text.at(m_position) == '\t'))
    {
        ++m_position;
    }
}

void Utils::Json::JsonReader::append_utf8(const uint32_t& in_code_point,
                                          std::string*    inout_string_ptr)
{
    constexpr uint32_t CONTINUATION_MARKER  = 0x80; /* marker bits of a continuation byte */
    constexpr uint32_t CONTINUATION_PAYLOAD = 0x3f; /* its payload bits                   */
    constexpr uint32_t N_PAYLOAD_BITS       = 6;

    struct Encoding
    {
        uint32_t limit;       /* code points below this take n_bytes bytes */
        uint32_t lead_marker; /* marker bits of the first byte             */
        uint32_t n_bytes;
    };

    constexpr Encoding ENCODING_ARRAY[] =
    {
        {0x80,     0x00, 1},
        {0x800,    0xc0, 2},
        {0x10000,  0xe0, 3},
        {0x110000, 0xf0, 4}
    };

    for (const auto& encoding : ENCODING_ARRAY)
    {
        if (in_code_point < encoding.limit)
        {
            for (uint32_t n_byte = 0;
                          n_byte < encoding.n_bytes;
                        ++n_byte)
            {
                const uint32_t n_shift_bits = (encoding.n_bytes - 1 - n_byte) * N_PAYLOAD_BITS;

                if (n_byte == 0)
                {
                    inout_string_ptr->push_back(static_cast<char>(encoding.lead_marker | (in_code_point >> n_shift_bits) ));
                }
                else
                {
                    inout_string_ptr->push_back(static_cast<char>(CONTINUATION_MARKER | ((in_code_point >> n_shift_bits) & CONTINUATION_PAYLOAD) ));
                }
            }

            break;
        }
    }
}

#include "fpl/LineParser.h"

#include "Utils.h"

#include <charconv>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>

namespace flint::fpl
{

constexpr const char* simpleTokens = "(),>:";

std::string fplErrorPrefix(const std::filesystem::path& path, int l, int c) noexcept
{
    return path.string() + "(" + std::to_string(l) + ", " + std::to_string(c) + "): ";
}

static bool isSimpleToken(char c) noexcept
{
    for (int i = 0; i < strlen(simpleTokens); ++i)
    {
        if (c == simpleTokens[i])
        {
            return true;
        }
    }
    return false;
}

LineParser::LineParser(const std::filesystem::path& path, std::string_view text, int number) noexcept :
    m_path(path), m_text(text), m_number(number)
{
}

void LineParser::lex() noexcept
{
    while (m_index < m_text.size())
    {
        Token t = processToken();
        // successfully reached end of line
        if (t.type == TokenType::COUNT)
        {
            return;
        }
        m_tokens.push_back(t);
    }
}

Token LineParser::processToken() noexcept
{
    Token t{};
    do
    {
        char c = m_text[m_index++];
        bool space = std::isspace(c);
        bool digit = std::isdigit(c);
        bool letter = std::isalpha(c);
        bool simple = isSimpleToken(c);
        bool hash = c == '#';
        bool dash = c == '-';
        bool under = c == '_';
        bool dot = c == '.';
        bool end = m_index > m_text.size();
        if (t.c < 0)
        {
            if (space)
            {
                continue;
            }
            else if (hash || end)
            {
                return t;
            }

            t.c = m_index;
            if (simple)
            {
                t.val = m_text.substr(t.c - 1, 1);
                switch (c)
                {
                case '(':
                    t.type = TokenType::OPEN_PAR;
                    return t;
                case ')':
                    t.type = TokenType::CLOSED_PAR;
                    return t;
                case '>':
                    t.type = TokenType::ARROW;
                    return t;
                case ',':
                    t.type = TokenType::COMMA;
                    return t;
                case ':':
                    t.type = TokenType::COLON;
                    return t;
                default:
                    fail(fplErrorPrefix(m_path, m_number, t.c) + "Invalid token, this shouldn't be happening");
                }
            }
            else if (under || letter)
            {
                t.type = TokenType::STRING;
                continue;
            }
            else if (digit || dash)
            {
                t.type = TokenType::INT;
                continue;
            }
            else if (dot)
            {
                t.type = TokenType::FLOAT;
                continue;
            }
        }
        else
        {
            bool terminating = space || simple || hash || end;
            if (terminating)
            {
                std::string_view token = m_text.substr(t.c - 1, m_index - t.c);
                --m_index;
                switch (t.type)
                {
                case TokenType::STRING:
                    t.val = token;
                    return t;
                case TokenType::INT:
                {
                    uint32_t i;
                    auto res = std::from_chars(token.data(), token.data() + token.size(), i);
                    if (res.ec == std::errc::invalid_argument)
                    {
                        fail(fplErrorPrefix(m_path, m_number, t.c) +
                             "Invalid syntax, token couldn't be converted to integer");
                    }
                    t.val = i;
                    return t;
                }
                case TokenType::FLOAT:
                {
                    float f;
                    auto res = std::from_chars(token.data(), token.data() + token.size(), f);
                    if (res.ec == std::errc::invalid_argument)
                    {
                        fail(fplErrorPrefix(m_path, m_number, t.c) +
                             "Invalid syntax, token couldn't be converted to floating-point");
                    }
                    t.val = f;
                    return t;
                }
                default:
                    fail(fplErrorPrefix(m_path, m_number, t.c) + "Invalid token, this shouldn't be happening");
                }
            }
            else if (dash)
            {
                fail(fplErrorPrefix(m_path, m_number, t.c) +
                     "Invalid syntax, dashes can only be found at the beginning of numbers");
            }
            else if (t.type != TokenType::STRING && letter)
            {
                fail(fplErrorPrefix(m_path, m_number, t.c) + "Invalid syntax, numbers can't contain letters");
            }
            else if (t.type != TokenType::STRING && under)
            {
                fail(fplErrorPrefix(m_path, m_number, t.c) + "Invalid syntax, numbers can't contain underscores");
            }
            else if (dot)
            {
                if (t.type == TokenType::INT)
                {
                    t.type = TokenType::FLOAT;
                    continue;
                }
                else if (t.type == TokenType::FLOAT)
                {
                    fail(fplErrorPrefix(m_path, m_number, t.c) +
                         "Invalid syntax, numbers can't contain more than one dot character");
                }
            }
        }
    } while (m_index <= m_text.size());
    return t;
}

bool LineParser::expect(const std::vector<TokenType>& ts) noexcept
{
    if (ts.size() > m_tokens.size() - m_index)
    {
        return false;
    }
    int i = m_index;
    for (auto t : ts)
    {
        if (t != m_tokens[i++].type)
        {
            return false;
        }
    }
    return true;
}

void LineParser::processInputsSection(LineInfo& info) noexcept
{
    if (!expect({TokenType::STRING}))
    {
        fail(fplErrorPrefix(m_path, m_number, m_tokens[m_index].c) +
             "Invalid syntax, line should start with least one input texture name");
    }
    info.inputs.push_back(m_tokens[m_index]);
    ++m_index;

    while (expect({TokenType::COMMA, TokenType::STRING}))
    {
        ++m_index;
        info.inputs.push_back(m_tokens[m_index]);
        ++m_index;
    }

    if (!expect({TokenType::ARROW}))
    {
        fail(fplErrorPrefix(m_path, m_number, m_tokens[m_index].c) +
             "Invalid syntax, arrow character (>) expected after input section");
    }
    ++m_index;
}

void LineParser::processFilterSection(LineInfo& info) noexcept
{
    if (!expect({TokenType::STRING}))
    {
        fail(fplErrorPrefix(m_path, m_number, m_tokens[m_index].c) + "Invalid syntax, filter call expected");
    }
    info.filterType = m_tokens[m_index];
    ++m_index;

    if (expect({TokenType::OPEN_PAR}))
    {
        ++m_index;
        if (expect({TokenType::STRING}))
        {
            info.namedParams = true;
            while (expect({TokenType::STRING, TokenType::COLON}))
            {
                std::string paramName{std::get<std::string_view>(m_tokens[m_index].val)};
                m_index += 2;
                if (expect({TokenType::INT}) || expect({TokenType::FLOAT}))
                {
                    info.params[paramName] = m_tokens[m_index];
                }
                else
                {
                    fail(fplErrorPrefix(m_path, m_number, m_tokens[m_index].c) + "Invalid argument, expected number");
                }
                ++m_index;
                if (expect({TokenType::COMMA}))
                {
                    ++m_index;
                }
                else
                {
                    break;
                }
            }
        }
        else if (expect({TokenType::INT}) || expect({TokenType::FLOAT}))
        {
            int index = 0;
            while (!expect({TokenType::CLOSED_PAR}))
            {
                if (expect({TokenType::INT}) || expect({TokenType::FLOAT}))
                {
                    info.params[std::to_string(index)] = m_tokens[m_index];
                }
                else
                {
                    fail(fplErrorPrefix(m_path, m_number, m_tokens[m_index].c) + "Invalid argument, expected number");
                }
                ++m_index;
                if (expect({TokenType::COMMA}))
                {
                    ++m_index;
                }
                else
                {
                    break;
                }
            }
        }

        if (!expect({TokenType::CLOSED_PAR}))
        {
            fail(fplErrorPrefix(m_path, m_number, m_tokens[m_index].c) +
                 "Invalid syntax, expected ')' to end filter argument list");
        }
        ++m_index;
    }

    if (expect({TokenType::INT}))
    {
        info.iterations = m_tokens[m_index];
        ++m_index;
    }

    if (!expect({TokenType::ARROW}))
    {
        fail(fplErrorPrefix(m_path, m_number, m_tokens[m_index].c) +
             "Invalid syntax, arrow to output section expected");
    }
    ++m_index;
}

void LineParser::processOutputSection(LineInfo& info) noexcept
{
    if (!expect({TokenType::STRING}))
    {
        fail(fplErrorPrefix(m_path, m_number, m_tokens[m_index].c) + "Invalid syntax, expected an output texture name");
    }
    info.output = m_tokens[m_index];
    ++m_index;

    if (m_index < m_tokens.size())
    {
        fail(fplErrorPrefix(m_path, m_number, m_tokens[m_index].c) +
             "Invalid syntax, expected only a single output texture name");
    }
}

LineInfo LineParser::parse() noexcept
{
    LineInfo info{};
    lex();
    if (m_tokens.empty())
    {
        info.empty = true;
        return info;
    }

    m_index = 0;
    processInputsSection(info);
    processFilterSection(info);
    processOutputSection(info);
    return info;
}
} // namespace flint::fpl
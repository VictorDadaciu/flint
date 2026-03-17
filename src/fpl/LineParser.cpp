#include "fpl/LineParser.h"

#include <charconv>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>

namespace flint::fpl
{

constexpr const char* simpleTokens = "(),>:";

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

void printError(const std::filesystem::path& path, int line, int col, const std::string& msg) noexcept
{
    std::cerr << "\nfpl ERROR " << path << " (" << line << ", " << col << "): " << msg << "\n";
}

#if 0
static std::string tokenTypeToStr(TokenType type) noexcept
{
    switch (type)
    {
    case TokenType::ARROW:
        return "ARROW";
    case TokenType::OPEN_PAR:
        return "OPEN_PAR";
    case TokenType::CLOSED_PAR:
        return "CLOSED_PAR";
    case TokenType::COMMA:
        return "COMMA";
    case TokenType::COLON:
        return "COLON";
    case TokenType::INT:
        return "INT";
    case TokenType::FLOAT:
        return "FLOAT";
    case TokenType::STRING:
        return "STRING";
    case TokenType::COUNT:
        return "COUNT";
    }
}

static void printToken(int lineNumber, const Token& token) noexcept
{
    std::cout << "{ " << "(" << lineNumber << ", " << token.c << "), " << tokenTypeToStr(token.type) << ", ";
    std::visit(
        [](const auto& value)
        {
            std::cout << "'" << value << "' } \n";
        },
        token.val);
}
#endif

LineParser::LineParser(const std::filesystem::path& path, std::string_view text, int number) noexcept :
    m_path(path), m_text(text), m_number(number)
{
}

bool LineParser::lex() noexcept
{
    while (m_index < m_text.size())
    {
        Token t{};
        if (!processToken(t))
        {
            return false;
        }
        // successfully reached end of line
        else if (t.type == TokenType::COUNT)
        {
            return true;
        }
        m_tokens.push_back(t);
    }
    return true;
}

bool LineParser::processToken(Token& t) noexcept
{
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
                return true;
            }

            t.c = m_index;
            if (simple)
            {
                t.val = m_text.substr(t.c - 1, 1);
                switch (c)
                {
                case '(':
                    t.type = TokenType::OPEN_PAR;
                    return true;
                case ')':
                    t.type = TokenType::CLOSED_PAR;
                    return true;
                case '>':
                    t.type = TokenType::ARROW;
                    return true;
                case ',':
                    t.type = TokenType::COMMA;
                    return true;
                case ':':
                    t.type = TokenType::COLON;
                    return true;
                default:
                    printError(m_path, m_number, t.c, "Invalid token, this shouldn't be happening");
                    return false;
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
                    return true;
                case TokenType::INT:
                {
                    uint32_t i;
                    auto res = std::from_chars(token.data(), token.data() + token.size(), i);
                    if (res.ec == std::errc::invalid_argument)
                    {
                        printError(m_path, m_number, t.c, "Invalid syntax, token couldn't be converted to integer");
                        return false;
                    }
                    t.val = i;
                    return true;
                }
                case TokenType::FLOAT:
                {
                    float f;
                    auto res = std::from_chars(token.data(), token.data() + token.size(), f);
                    if (res.ec == std::errc::invalid_argument)
                    {
                        printError(
                            m_path, m_number, t.c, "Invalid syntax, token couldn't be converted to floating-point");
                        return false;
                    }
                    t.val = f;
                    return true;
                }
                default:
                    printError(m_path, m_number, t.c, "Invalid token, this shouldn't be happening");
                    return false;
                }
            }
            else if (dash)
            {
                printError(
                    m_path, m_number, t.c, "Invalid syntax, dashes can only be found at the beginning of numbers");
                return false;
            }
            else if (t.type != TokenType::STRING && letter)
            {
                printError(m_path, m_number, t.c, "Invalid syntax, numbers can't contain letters");
                return false;
            }
            else if (t.type != TokenType::STRING && under)
            {
                printError(m_path, m_number, t.c, "Invalid syntax, numbers can't contain underscores");
                return false;
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
                    printError(
                        m_path, m_number, t.c, "Invalid syntax, numbers can't contain more than one dot character");
                    return false;
                }
            }
        }
    } while (m_index <= m_text.size());
    return true;
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

bool LineParser::processInputsSection(LineInfo& info) noexcept
{
    if (!expect({TokenType::STRING}))
    {
        printError(m_path,
                   m_number,
                   m_tokens[m_index].c,
                   "Invalid syntax, line should start with least one input texture name");
        return false;
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
        printError(
            m_path, m_number, m_tokens[m_index].c, "Invalid syntax, arrow character (>) expected after input section");
        return false;
    }
    ++m_index;
    return true;
}

bool LineParser::processFilterSection(LineInfo& info) noexcept
{
    if (!expect({TokenType::STRING, TokenType::OPEN_PAR}))
    {
        printError(m_path, m_number, m_tokens[m_index].c, "Invalid syntax, filter call expected");
        return false;
    }
    info.filterType = m_tokens[m_index];
    m_index += 2;

    while (expect({TokenType::STRING, TokenType::COLON}))
    {
        std::string_view paramName = std::get<std::string_view>(m_tokens[m_index].val);
        m_index += 2;
        if (expect({TokenType::INT}) || expect({TokenType::FLOAT}))
        {
            info.params[paramName] = m_tokens[m_index];
        }
        else
        {
            printError(m_path, m_number, m_tokens[m_index].c, "Invalid argument, expected number");
            return false;
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

    if (!expect({TokenType::CLOSED_PAR}))
    {
        printError(m_path, m_number, m_tokens[m_index].c, "Invalid syntax, expected ')' to end filter argument list");
        return false;
    }
    ++m_index;

    if (expect({TokenType::INT}))
    {
        info.iterations = m_tokens[m_index];
        ++m_index;
    }

    if (!expect({TokenType::ARROW}))
    {
        printError(m_path, m_number, m_tokens[m_index].c, "Invalid syntax, arrow to output section expected");
        return false;
    }
    ++m_index;
    return true;
}

bool LineParser::processOutputSection(LineInfo& info) noexcept
{
    if (!expect({TokenType::STRING}))
    {
        printError(m_path, m_number, m_tokens[m_index].c, "Invalid syntax, expected an output texture name");
        return false;
    }
    info.output = m_tokens[m_index];
    ++m_index;

    if (m_index < m_tokens.size())
    {
        printError(m_path, m_number, m_tokens[m_index].c, "Invalid syntax, expected only a single output texture name");
        return false;
    }
    return true;
}

bool LineParser::parse(LineInfo& info) noexcept
{
    info.number = m_number;
    if (!lex())
    {
        return false;
    }
    if (m_tokens.empty())
    {
        info.empty = true;
        return true;
    }

#if 0
    for (const auto& t : m_tokens)
    {
        printToken(info.number, t);
    }
    std::cout << "\n";
#endif

    m_index = 0;
    return processInputsSection(info) && processFilterSection(info) && processOutputSection(info);
}
} // namespace flint::fpl
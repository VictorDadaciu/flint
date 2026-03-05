#include "FilterPipelineFileParser.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace flint
{
FilterPipelineFileParser::FilterPipelineFileParser(const std::string& path) : m_path(path)
{
}

std::vector<std::string> FilterPipelineFileParser::toLines() const noexcept
{
    std::ifstream f(m_path);
    if (!f)
    {
        return {};
    }
    std::vector<std::string> lines{};
    std::string line;
    while (std::getline(f, line))
    {
        lines.push_back(line);
    }
    return lines;
}

class Line
{
public:
    Line(const std::string& line, int lineNumber) noexcept : m_text(line), m_line(lineNumber) {}

    inline int lineNumber() const noexcept { return m_line; }

    inline int columnNumber() const noexcept { return m_column; }

    inline bool empty() const noexcept { return m_column >= m_text.size(); }

    inline char pop() noexcept { return m_text[m_column++]; }

private:
    const std::string& m_text;
    const int m_line;
    int m_column{};
};

struct TokenConstructor
{
    TokenType type = TokenType::COUNT;
    std::string value{};
};

static void addSingleCharTokenIfNeeded(char c, std::vector<Token>& tokens) noexcept
{
    if (c == '(')
    {
        tokens.push_back(Token{.type = TokenType::OPEN_PAR});
        return;
    }
    if (c == ')')
    {
        tokens.push_back(Token{.type = TokenType::CLOSED_PAR});
        return;
    }
    if (c == ',')
    {
        tokens.push_back(Token{.type = TokenType::COMMA});
        return;
    }
    if (c == '>')
    {
        tokens.push_back(Token{.type = TokenType::ARROW});
        return;
    }
}

static void printError(const std::string& path, const Line& line, const std::string& errorMessage) noexcept
{
    std::cout << "ERROR in " << path << " at line " << line.lineNumber() << " around column " << line.columnNumber()
              << "): " << errorMessage;
}

std::vector<Token> FilterPipelineFileParser::lex() const noexcept
{
    std::vector<std::string> lines(toLines());
    if (lines.empty())
    {
        return {};
    }

    std::vector<Token> tokens{};
    for (int l = 0; l < lines.size(); ++l)
    {
        TokenConstructor token{};
        Line line{lines[l], l};
        while (!line.empty())
        {
            char c = line.pop();
            if (c == '(' || c == ')' || c == ',' || c == '>' || std::isspace(c))
            {
                if (!token.value.empty())
                {
                    if (token.value == "input")
                    {
                        tokens.push_back(Token{.type = TokenType::INPUT});
                    }
                    else if (token.value == "output")
                    {
                        tokens.push_back(Token{.type = TokenType::OUTPUT});
                    }
                    else
                    {
                        Token newToken{.type = token.type};
                        switch (newToken.type)
                        {
                        case TokenType::STRING:
                            newToken.val = token.value;
                            break;
                        case TokenType::INT:
                            newToken.val = atoi(token.value.c_str());
                            break;
                        case TokenType::FLOAT:
                            newToken.val = (float)atof(token.value.c_str());
                            break;
                        default:
                            return {};
                        }
                        tokens.push_back(newToken);
                    }
                }
                token = TokenConstructor();
                addSingleCharTokenIfNeeded(c, tokens);
                continue;
            }

            if (token.value.empty())
            {
                if (std::isdigit(c))
                {
                    token.type = TokenType::INT;
                    token.value += c;
                    continue;
                }
                if (c == '.')
                {
                    token.type = TokenType::FLOAT;
                    token.value += "0.";
                    continue;
                }
                if (std::isalpha(c))
                {
                    token.type = TokenType::STRING;
                    token.value += c;
                    continue;
                }
            }
            else
            {
                if (std::isdigit(c))
                {
                    token.value += c;
                    continue;
                }
                if (std::isalpha(c))
                {
                    if (token.type == TokenType::STRING)
                    {
                        token.value += c;
                        continue;
                    }
                    else
                    {
                        printError(m_path, line, "Strings cannot start with digits");
                        return {};
                    }
                }
                if (c == '.')
                {
                    switch (token.type)
                    {
                    case TokenType::STRING:
                        token.value += '.';
                        break;
                    case TokenType::INT:
                        token.type = TokenType::FLOAT;
                        token.value += '.';
                        break;
                    case TokenType::FLOAT:
                        printError(m_path, line, "Numbers cannot contain more than one . (dot) character");
                        return {};
                    default:
                    }
                    continue;
                }
            }
            printError(m_path, line, "Unrecognized character");
        }
        if (!token.value.empty())
        {
            if (token.value == "input")
            {
                tokens.push_back(Token{.type = TokenType::INPUT});
            }
            else if (token.value == "output")
            {
                tokens.push_back(Token{.type = TokenType::OUTPUT});
            }
            else
            {
                Token newToken{.type = token.type};
                switch (newToken.type)
                {
                case TokenType::STRING:
                    newToken.val = token.value;
                    break;
                case TokenType::INT:
                    newToken.val = atoi(token.value.c_str());
                    break;
                case TokenType::FLOAT:
                    newToken.val = (float)atof(token.value.c_str());
                    break;
                default:
                    return {};
                }
                tokens.push_back(newToken);
            }
        }
    }
    return tokens;
}

std::vector<Token> FilterPipelineFileParser::parse() const noexcept
{
    return lex();
}
} // namespace flint
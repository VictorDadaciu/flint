#pragma once

#include <string>
#include <variant>
#include <vector>

namespace flint
{
enum class TokenType
{
    INPUT,
    OUTPUT,
    OPEN_PAR,
    CLOSED_PAR,
    ARROW,
    COMMA,
    STRING,
    INT,
    FLOAT,
    COUNT
};

// TODO add line and column
struct Token final
{
    TokenType type = TokenType::COUNT;
    std::variant<std::string, int, float> val{};
};

class FilterPipelineFileParser final
{
public:
    FilterPipelineFileParser(const std::string&);

    std::vector<Token> parse() const noexcept;

private:
    std::vector<std::string> toLines() const noexcept;
    std::vector<Token> lex() const noexcept;

    std::string m_path{};
};
} // namespace flint
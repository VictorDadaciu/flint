#pragma once

#include <filesystem>
#include <map>
#include <string_view>
#include <variant>
#include <vector>

namespace flint::fpl
{
void printError(const std::filesystem::path&, int, int, const std::string&) noexcept;

enum class TokenType
{
    OPEN_PAR,
    CLOSED_PAR,
    ARROW,
    COMMA,
    COLON,
    STRING,
    INT,
    FLOAT,
    COUNT
};

struct Token final
{
    TokenType type = TokenType::COUNT;

    std::variant<std::string_view, uint32_t, float> val{};

    int c = -1;
};

struct LineInfo final
{
    std::vector<Token> inputs{};
    Token filterType{};
    Token output{};
    std::map<std::string_view, Token> params{};
    Token iterations{};
    int number{};
    bool empty{};
};

class LineParser final
{
public:
    LineParser(const std::filesystem::path&, std::string_view, int) noexcept;

    bool parse(LineInfo&) noexcept;

private:
    bool lex() noexcept;

    bool processToken(Token&) noexcept;

    bool expect(const std::vector<TokenType>&) noexcept;

    bool processInputsSection(LineInfo&) noexcept;
    bool processFilterSection(LineInfo&) noexcept;
    bool processOutputSection(LineInfo&) noexcept;

    std::filesystem::path m_path;
    std::string_view m_text;
    const int m_number;
    std::vector<Token> m_tokens{};
    int m_index{};
};
} // namespace flint::fpl
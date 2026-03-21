#pragma once

#include <filesystem>
#include <map>
#include <string_view>
#include <variant>
#include <vector>

namespace flint::fpl
{
std::string fplErrorPrefix(const std::filesystem::path&, int, int) noexcept;

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
    std::map<std::string, Token> params{};
    bool namedParams{};
    Token iterations{};
    Token output{};
    bool empty{};
};

class LineParser final
{
public:
    LineParser(const std::filesystem::path&, std::string_view, int) noexcept;

    LineInfo parse() noexcept;

private:
    void lex() noexcept;

    Token processToken() noexcept;

    bool expect(const std::vector<TokenType>&) noexcept;

    void processInputsSection(LineInfo&) noexcept;
    void processFilterSection(LineInfo&) noexcept;
    void processOutputSection(LineInfo&) noexcept;

    std::filesystem::path m_path;
    std::string_view m_text;
    const int m_number;
    std::vector<Token> m_tokens{};
    int m_index{};
};
} // namespace flint::fpl
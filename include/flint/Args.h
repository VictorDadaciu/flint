#pragma once

#include <filesystem>
#include <optional>
#include <unordered_map>
#include <variant>

namespace flint
{
struct Args
{
    std::filesystem::path inputPath{};
    std::filesystem::path outputPath{};
    std::string filter{};
    std::optional<bool> noOverwrite{};
    std::unordered_map<std::string, std::variant<uint32_t, float>> params{{"radius", 3u}};
};

namespace args
{
    [[nodiscard]] Args parse(int, const char**) noexcept;
}
} // namespace flint
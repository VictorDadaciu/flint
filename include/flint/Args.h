#pragma once

#include <filesystem>
#include <optional>

namespace flint
{
struct Args
{
    std::filesystem::path inputPath{};
    std::filesystem::path outputPath{};
    std::filesystem::path filterPath{};
    std::optional<bool> noOverwrite{};
};

namespace args
{
    [[nodiscard]] Args parse(int, const char**) noexcept;
}
} // namespace flint
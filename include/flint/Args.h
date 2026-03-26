#pragma once

#include <filesystem>

namespace flint
{
struct Args
{
    std::filesystem::path inputPath{};
    std::filesystem::path outputPath{};
    std::filesystem::path filterPath{};
    bool noOverwrite{};
    bool verbose{};
};

namespace args
{
    [[nodiscard]] Args parse(int, const char**) noexcept;
}
} // namespace flint
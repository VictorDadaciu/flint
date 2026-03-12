#pragma once

#include "FilterUtils.h"
#include "cmdparser.hpp"

#include <filesystem>
#include <string>

namespace flint::fpl
{
struct Params final
{
    void* data{};
    size_t size{};
};

struct FilterSlot
{
    FilterType type = FilterType::count;
    std::vector<int> inputs{}; // first texture placeholder, then filter slot
    int outputTexture = -1;
    int height = -1;
    Params params{};
    bool firstIteration = true;
};

struct PipelineLayout final
{
    PipelineLayout(const cli::Parser&) noexcept;

    inline bool valid() const noexcept { return m_valid; }

    void cleanup() noexcept;

    std::vector<FilterSlot> slots{};
    int texCount{};

private:
    bool createFromFilterName(const std::string&) noexcept;

    bool loadFplFromSource(const std::filesystem::path&) noexcept;

    bool m_valid{};
};
} // namespace flint::fpl
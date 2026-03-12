#pragma once

#include "FilterUtils.h"
#include "cmdparser.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace flint::fpl
{
struct FilterSlot
{
    FilterType type = FilterType::count;
    std::vector<int> inputs{}; // first texture placeholder, then filter slot
    int outputTexture = -1;
    int height = -1;

    struct
    {
        std::vector<bool> isFloat{};
        std::vector<uint32_t> vals{};
    } params{};
};

struct PipelineLayout final
{
    PipelineLayout(const cli::Parser&) noexcept;

    inline bool valid() const noexcept { return m_valid; }

    std::vector<FilterSlot> slots{};
    int texCount{};

private:
    bool createFromFilterName(const std::string&) noexcept;

    bool loadFplFromSource(const std::filesystem::path&) noexcept;

    bool serializeToCache(const std::string&) const noexcept;

    bool deserializeFromCache(const std::string&) noexcept;

    bool m_valid{};
};
} // namespace flint::fpl
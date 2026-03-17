#pragma once

#include "FilterInstance.h"
#include "cmdparser.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace flint::fpl
{
struct FilterSlot
{
    std::string filterName{};
    std::vector<int> inputs{}; // first texture placeholder, then filter slot
    int outputTexture = -1;
    int height = -1;

    std::vector<uint32_t> params{};
};

struct PipelineLayout final
{
    PipelineLayout(const cli::Parser&) noexcept;

    inline bool valid() const noexcept { return m_valid; }

    void cleanup() noexcept;

    std::unordered_map<std::string, std::unique_ptr<FilterInstance>> instances{};
    std::vector<FilterSlot> slots{};
    int texCount{};

private:
    bool createFromFilterName(const cli::Parser&) noexcept;

    bool loadFplFromSource(const std::filesystem::path&) noexcept;

    bool serializeToCache(const std::string&) const noexcept;

    bool deserializeFromCache(const std::filesystem::path&) noexcept;

    bool m_valid{};
};
} // namespace flint::fpl
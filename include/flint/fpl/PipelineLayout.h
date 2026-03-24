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

// TODO add constructors/destructors
struct PipelineLayout final
{
    PipelineLayout(const cli::Parser&) noexcept;

    PipelineLayout(PipelineLayout&) = delete;
    PipelineLayout(PipelineLayout&&) noexcept;

    void operator=(PipelineLayout&) = delete;
    void operator=(PipelineLayout&&) noexcept;

    ~PipelineLayout() = default;

    std::unordered_map<std::string, std::unique_ptr<FilterInstance>> instances{};
    std::vector<FilterSlot> slots{};
    int texCount{};

private:
    void createFromFilterName(const cli::Parser&) noexcept;

    void loadFplFromSource(const std::filesystem::path&) noexcept;
};
} // namespace flint::fpl
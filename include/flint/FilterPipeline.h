#pragma once

#include "FilterInstance.h"
#include "FilterUtils.h"
#include "SubmissionStack.h"
#include "Texture.h"
#include "cmdparser.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace flint
{
struct FilterSlot
{
    std::vector<int> inputFilterSlots{};
    int outputTexture = -1;
    FilterType type = FilterType::count;
    int height = -1;
    int paramsSize{};
    void* paramsData{};
    bool firstIteration = true;
};

class FilterInstance;

class FilterPipeline
{
public:
    FilterPipeline(const cli::Parser&) noexcept;

    bool record(std::vector<Texture>&, SubmissionStack&) const noexcept;

    void cleanup() noexcept;

    inline bool valid() const noexcept { return m_valid; }

    inline int uniqueTexturesCount() const noexcept { return m_uniqueTexturesCount; }

private:
    bool createSimplePipeline(const std::string&) noexcept;

    bool createComplexPipeline(const std::string&) noexcept;

    bool applyFilter(std::vector<Texture>&, const FilterSlot&, SubmissionStack&) const noexcept;

    mutable std::unordered_map<FilterType, std::unique_ptr<FilterInstance>> m_filterInstances{};
    std::vector<FilterSlot> m_filterSlots{};
    int m_uniqueTexturesCount{};
    bool m_valid{};
};
} // namespace flint
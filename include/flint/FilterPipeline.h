#pragma once

#include "Args.h"
#include "FilterInstance.h"
#include "FilterUtils.h"
#include "SubmissionStack.h"
#include "Texture.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace flint
{
struct FilterSlot
{
    std::vector<int> inputFilterSlots{};
    int outputTexture = -1;
    FilterType type = FilterType::count;
};

class FilterInstance;

class FilterPipeline
{
public:
    FilterPipeline(const Args&) noexcept;

    bool record(std::vector<Texture>&, SubmissionStack&) const noexcept;

    void cleanup() noexcept;

    inline bool valid() const noexcept { return m_valid; }

    inline int uniqueTexturesCount() const noexcept { return m_uniqueTexturesCount; }

private:
    bool applyFilter(std::vector<Texture>&, const FilterSlot&, SubmissionStack&) const noexcept;

    mutable std::unordered_map<FilterType, std::unique_ptr<FilterInstance>> m_filterInstances{};
    std::vector<FilterSlot> m_filterSlots{};
    int m_uniqueTexturesCount{};
    bool m_valid{};
};
} // namespace flint
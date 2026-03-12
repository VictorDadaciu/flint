#pragma once

#include "FilterInstance.h"
#include "FilterUtils.h"
#include "SubmissionStack.h"
#include "Texture.h"
#include "cmdparser.hpp"
#include "fpl/PipelineLayout.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace flint
{
class FilterInstance;

class FilterPipeline
{
public:
    FilterPipeline(const cli::Parser&) noexcept;

    bool record(std::vector<Texture>&, SubmissionStack&) const noexcept;

    void cleanup() noexcept;

    inline bool valid() const noexcept { return m_valid; }

    inline int texCount() const noexcept { return m_layout.texCount; }

private:
    bool createSimplePipeline(const std::string&) noexcept;

    bool createComplexPipeline(const std::filesystem::path&) noexcept;

    bool applyFilter(std::vector<Texture>&, const fpl::FilterSlot&, SubmissionStack&) const noexcept;

    mutable std::unordered_map<FilterType, std::unique_ptr<FilterInstance>> m_filterInstances{};
    fpl::PipelineLayout m_layout;
    bool m_valid{};
};
} // namespace flint
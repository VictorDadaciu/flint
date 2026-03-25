#pragma once

#include "Args.h"
#include "SubmissionStack.h"
#include "Texture.h"
#include "fpl/PipelineLayout.h"

#include <vector>

namespace flint
{
class FilterInstance;

class FilterPipeline final
{
public:
    FilterPipeline(const Args&) noexcept;

    FilterPipeline(FilterPipeline&) = delete;

    FilterPipeline(FilterPipeline&&) noexcept;

    void operator=(FilterPipeline&) = delete;

    void operator=(FilterPipeline&&) noexcept;

    ~FilterPipeline() = default;

    void record(std::vector<Texture>&, SubmissionStack&) const noexcept;

    inline int texCount() const noexcept { return m_layout.texCount; }

private:
    void applyFilter(std::vector<Texture>&, const fpl::FilterSlot&, SubmissionStack&) const noexcept;

    fpl::PipelineLayout m_layout;
};
} // namespace flint
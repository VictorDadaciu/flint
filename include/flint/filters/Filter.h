#pragma once

#include "Args.h"
#include "Texture.h"
#include "stages/FilterStage.h"

#include <memory>
#include <vector>

namespace flint::detail
{
class Filter
{
public:
    inline bool valid() const noexcept { return m_valid; }

    bool run();

    virtual ~Filter() = default;

protected:
    void cleanup() noexcept
    {
        m_valid = false;
        for (int i = 0; i < m_stages.size(); ++i)
        {
            m_stages[i]->cleanup();
        }
        for (int i = 0; i < m_texes.size(); ++i)
        {
            m_texes[i].cleanup();
        }
    }

    std::vector<std::unique_ptr<FilterStage>> m_stages{};
    std::vector<vulkan::Texture> m_texes{};
    bool m_valid{};
};
} // namespace flint::detail

namespace flint
{
bool applyFilters(const Args&) noexcept;
} // namespace flint
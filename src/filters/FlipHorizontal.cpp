#include "filters/FlipHorizontal.h"

#include "stages/CopyImageToPCStage.h"
#include "stages/FilterOp.h"
#include "stages/FilterStage.h"
#include "stages/LoadImageStage.h"

#include <array>
#include <memory>

namespace flint
{
FlipHorizontal::FlipHorizontal(const Args& args) noexcept
{
    m_stages.resize(3);
    m_stages[0] = std::make_unique<vulkan::LoadImageStage>(args);
    {
        std::array<FilterStage*, 1> inputs{m_stages[0].get()};
        m_stages[1] = std::make_unique<vulkan::FilterOp<1>>(
            "/home/victordadaciu/workspace/flint/compute/flip_h.comp.spv", inputs, true);
    }
    m_stages[2] = std::make_unique<vulkan::CopyImageToPCStage>(m_stages[1].get());
    for (int i = 0; i < m_stages.size(); ++i)
    {
        if (!m_stages[i]->valid())
        {
            cleanup();
            return;
        }
    }
    m_valid = true;
}
} // namespace flint
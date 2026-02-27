#include "filters/Sobel.h"

#include "stages/CopyImageToPCStage.h"
#include "stages/FilterOp.h"
#include "stages/FilterStage.h"
#include "stages/GenerateImageStage.h"
#include "stages/LoadImageStage.h"

#include <array>
#include <memory>

namespace flint
{
Sobel::Sobel(const Args& args) noexcept
{
    m_texes.resize(2);
    m_stages.resize(4);
    m_stages[0] = std::make_unique<vulkan::LoadImageStage>(m_texes[0], args);
    m_stages[1] = std::make_unique<vulkan::GenerateImageStage>(m_texes[1]);
    {
        std::array<FilterStage*, 1> inputs{m_stages[0].get()};
        m_stages[2] = std::make_unique<vulkan::FilterOp<1>>(
            "/home/victordadaciu/workspace/flint/compute/sobel.comp.spv", inputs, m_stages[1].get());
    }
    m_stages[3] = std::make_unique<vulkan::CopyImageToPCStage>(m_stages[2].get());
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
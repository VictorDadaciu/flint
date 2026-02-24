#pragma once

#include "Texture.h"

#include <vector>
#include <vulkan/vulkan.h>

namespace flint
{
class FilterStage
{
public:
    bool valid() const noexcept { return m_valid; }

    virtual inline const VkSemaphore& signal() const noexcept = 0;

    virtual inline const vulkan::Texture& tex() const noexcept = 0;

    virtual inline std::vector<VkSubmitInfo> submitInfos() const noexcept = 0;

    virtual void cleanup() noexcept = 0;

    virtual ~FilterStage() = default;

protected:
    bool m_valid{};
};
} // namespace flint
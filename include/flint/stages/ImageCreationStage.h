#pragma once

#include "Texture.h"
#include "stages/FilterStage.h"

#include <vector>

namespace flint::vulkan
{
class ImageCreationStage : public FilterStage
{
public:
    ImageCreationStage() = default;

    inline const Texture& tex() const noexcept override { return m_tex; }

    virtual inline const VkSemaphore& signal() const noexcept override = 0;

    virtual inline std::vector<VkSubmitInfo> submitInfos() const noexcept override = 0;

    virtual void cleanup() noexcept override;

protected:
    bool createImage(VkImageUsageFlags) noexcept;

    bool createImageView() noexcept;

    Texture m_tex{};
};
} // namespace flint::vulkan
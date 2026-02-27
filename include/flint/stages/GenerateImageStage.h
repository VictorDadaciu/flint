#pragma once

#include "ImageCreationStage.h"
#include "Submission.h"
#include "Texture.h"

namespace flint::vulkan
{
class GenerateImageStage final : public ImageCreationStage
{
public:
    GenerateImageStage(Texture& tex) noexcept;

    void cleanup() noexcept override;

    inline const VkSemaphore& signal() const noexcept override { return m_generalTransitionSubmission.semaphore; };

    inline std::vector<VkSubmitInfo> submitInfos() const noexcept override
    {
        return {m_generalTransitionSubmission.info};
    }

private:
    bool transitionImageToGeneral() noexcept;

    Submission m_generalTransitionSubmission{};
};
} // namespace flint::vulkan
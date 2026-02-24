#pragma once

#include "Args.h"
#include "ImageCreationStage.h"
#include "Submission.h"

#include <vulkan/vulkan.h>

namespace flint::vulkan
{
class LoadImageStage final : public ImageCreationStage
{
public:
    LoadImageStage(const Args&);

    void cleanup() noexcept override;

    inline const VkSemaphore& signal() const noexcept override { return m_generalTransitionSubmission.semaphore; };

    inline std::vector<VkSubmitInfo> submitInfos() const noexcept override
    {
        return {m_transferTransitionSubmission.info, m_copySubmission.info, m_generalTransitionSubmission.info};
    }

private:
    bool transitionImageToTransferDestination() noexcept;

    bool copyBufferToImage() noexcept;

    bool transitionImageToGeneral() noexcept;

    Submission m_transferTransitionSubmission{};
    Submission m_copySubmission{};
    Submission m_generalTransitionSubmission{};
};
} // namespace flint::vulkan
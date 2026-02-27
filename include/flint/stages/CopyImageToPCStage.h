#pragma once

#include "Submission.h"
#include "Texture.h"
#include "stages/FilterStage.h"

namespace flint::vulkan
{
class CopyImageToPCStage final : public FilterStage
{
public:
    CopyImageToPCStage(FilterStage*) noexcept;

    inline std::vector<VkSubmitInfo> submitInfos() const noexcept override
    {
        return {m_transferTransitionSubmission.info, m_copySubmission.info};
    };

    inline const Texture& tex() const noexcept override { return m_input->tex(); }

    inline const VkSemaphore& signal() const noexcept override { return m_copySubmission.semaphore; }

    void cleanup() noexcept override;

private:
    bool transitionImageToTransferSource() noexcept;

    bool copyImageToBuffer() noexcept;

    FilterStage* m_input; // won't clean, non-owning
    Submission m_transferTransitionSubmission{};
    Submission m_copySubmission{};
};
} // namespace flint::vulkan
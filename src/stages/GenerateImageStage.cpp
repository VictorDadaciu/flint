#include "stages/GenerateImageStage.h"

#include "VkContext.h"
#include "stages/ImageCreationStage.h"

#include <vulkan/vulkan_core.h>

namespace flint::vulkan
{
GenerateImageStage::GenerateImageStage(bool last) noexcept
{
    if (!(createImage(last ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0) && createImageView() && transitionImageToGeneral()))
    {
        cleanup();
        return;
    }
    m_valid = true;
}

void GenerateImageStage::cleanup() noexcept
{
    ImageCreationStage::cleanup();
    vkDestroySemaphore(ctx->device, m_generalTransitionSubmission.semaphore, nullptr);
}

bool GenerateImageStage::transitionImageToGeneral() noexcept
{
    if (!m_generalTransitionSubmission.begin())
    {
        return false;
    }

    auto barrier = createImageMemoryBarrier();
    barrier.image = m_tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(m_generalTransitionSubmission.commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    return m_generalTransitionSubmission.end();
}
} // namespace flint::vulkan
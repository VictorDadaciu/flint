#include "stages/CopyImageToPCStage.h"

#include "StagingBuffer.h"
#include "VkContext.h"
#include "stages/FilterStage.h"

#include <vulkan/vulkan_core.h>

namespace flint::vulkan
{
CopyImageToPCStage::CopyImageToPCStage(FilterStage* stage) noexcept : m_tex(stage->tex())
{
    if (!(transitionImageToTransferSource(stage->signal()) && copyImageToBuffer()))
    {
        cleanup();
    }
    m_valid = true;
}

void CopyImageToPCStage::cleanup() noexcept
{
    m_valid = false;
    vkDestroySemaphore(ctx->device, m_copySubmission.semaphore, nullptr);
    vkDestroySemaphore(ctx->device, m_transferTransitionSubmission.semaphore, nullptr);
}

bool CopyImageToPCStage::transitionImageToTransferSource(const VkSemaphore& wait) noexcept
{
    if (!m_transferTransitionSubmission.begin())
    {
        return false;
    }

    auto barrier = createImageMemoryBarrier();
    barrier.image = m_tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(m_transferTransitionSubmission.commandBuffer,
                         sourceStage,
                         destinationStage,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    m_transferTransitionSubmission.waitSemaphores.push_back(wait);
    m_transferTransitionSubmission.waitFlags.push_back(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    return m_transferTransitionSubmission.end();
}

bool CopyImageToPCStage::copyImageToBuffer() noexcept
{
    if (!m_copySubmission.begin())
    {
        return false;
    }

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {(uint32_t)imageMetadata.width, (uint32_t)imageMetadata.height, 1};

    vkCmdCopyImageToBuffer(m_copySubmission.commandBuffer,
                           m_tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           stagingBuffer.m_buffer,
                           1,
                           &region);

    m_copySubmission.waitSemaphores.push_back(m_transferTransitionSubmission.semaphore);
    m_copySubmission.waitFlags.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
    return m_copySubmission.end();
}
} // namespace flint::vulkan
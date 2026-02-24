#include "stages/LoadImageStage.h"

#include "StagingBuffer.h"
#include "Texture.h"
#include "VkContext.h"
#include "stages/ImageCreationStage.h"

#include <iostream>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace flint::vulkan
{
static unsigned char* loadImage(const char* path) noexcept
{
    int channels{};
    unsigned char* raw = stbi_load(path, &imageMetadata.width, &imageMetadata.height, &channels, STBI_rgb_alpha);
    if (!raw)
    {
        std::cout << "Failed to load image at path " << path << ": " << stbi_failure_reason() << "\n";
        return nullptr;
    }
    imageMetadata.groupX =
        imageMetadata.width / LOCAL_WORK_SIZE + 1 * (int)((bool)(imageMetadata.width % LOCAL_WORK_SIZE));
    imageMetadata.groupY =
        imageMetadata.height / LOCAL_WORK_SIZE + 1 * (int)((bool)(imageMetadata.height % LOCAL_WORK_SIZE));

    return raw;
}

void LoadImageStage::cleanup() noexcept
{
    ImageCreationStage::cleanup();
    vkDestroySemaphore(ctx->device, m_generalTransitionSubmission.semaphore, nullptr);
    vkDestroySemaphore(ctx->device, m_copySubmission.semaphore, nullptr);
    vkDestroySemaphore(ctx->device, m_transferTransitionSubmission.semaphore, nullptr);
}

LoadImageStage::LoadImageStage(const Args& args)
{
    if (!stagingBuffer.createFromRawImage(loadImage(args.path)))
    {
        cleanup();
        return;
    }

    if (!(createImage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT) &&
          createImageView() &&
          transitionImageToTransferDestination() &&
          copyBufferToImage() &&
          transitionImageToGeneral()))
    {
        cleanup();
        return;
    }
    m_valid = true;
}

bool LoadImageStage::transitionImageToTransferDestination() noexcept
{
    if (!m_transferTransitionSubmission.begin())
    {
        return false;
    }

    auto barrier = createImageMemoryBarrier();
    barrier.image = m_tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(m_transferTransitionSubmission.commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    return m_transferTransitionSubmission.end();
}

bool LoadImageStage::copyBufferToImage() noexcept
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

    vkCmdCopyBufferToImage(m_copySubmission.commandBuffer,
                           stagingBuffer.m_buffer,
                           m_tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);

    m_copySubmission.waitSemaphores.push_back(m_transferTransitionSubmission.semaphore);
    m_copySubmission.waitFlags.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
    return m_copySubmission.end();
}

bool LoadImageStage::transitionImageToGeneral() noexcept
{
    if (!m_generalTransitionSubmission.begin())
    {
        return false;
    }

    auto barrier = createImageMemoryBarrier();
    barrier.image = m_tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(m_generalTransitionSubmission.commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    m_generalTransitionSubmission.waitSemaphores.push_back(m_copySubmission.semaphore);
    m_generalTransitionSubmission.waitFlags.push_back(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    return m_generalTransitionSubmission.end();
}
} // namespace flint::vulkan
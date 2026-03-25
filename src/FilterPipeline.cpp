#include "FilterPipeline.h"

#include "Args.h"
#include "FilterInstance.h"
#include "StagingBuffer.h"
#include "SubmissionInfo.h"
#include "SubmissionStack.h"
#include "Texture.h"
#include "VkContext.h"

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace flint
{
FilterPipeline::FilterPipeline(const Args& args) noexcept : m_layout(args)
{
}

FilterPipeline::FilterPipeline(FilterPipeline&& other) noexcept : m_layout(std::move(other.m_layout))
{
}

void FilterPipeline::operator=(FilterPipeline&& other) noexcept
{
    m_layout = std::move(other.m_layout);
}

static void transitionToTransfer(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    submissions[transfer].begin(VK_PIPELINE_STAGE_TRANSFER_BIT);

    auto barrier = createImageMemoryBarrier();
    barrier.image = tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(submissions[transfer].commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    tex.lastSubmissionIndex = transfer;
    submissions[transfer].end();
}

static void copyImage(Texture& tex, SubmissionStack& submissions) noexcept
{
    int copyImage = submissions.get();
    submissions[copyImage].begin();

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

    vkCmdCopyBufferToImage(submissions[copyImage].commandBuffer,
                           stagingBuffer.buffer,
                           tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);

    submissions[copyImage].prereqs.push_back(tex.lastSubmissionIndex);
    tex.lastSubmissionIndex = copyImage;
    submissions[copyImage].end();
}

static void transitionToComputeFromTransfer(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    submissions[transfer].begin();

    auto barrier = createImageMemoryBarrier();
    barrier.image = tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(submissions[transfer].commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    submissions[transfer].prereqs.push_back(tex.lastSubmissionIndex);
    tex.lastSubmissionIndex = transfer;
    submissions[transfer].end();
}

static void transitionToComputeFromUndefined(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    submissions[transfer].begin();

    auto barrier = createImageMemoryBarrier();
    barrier.image = tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(submissions[transfer].commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    tex.lastSubmissionIndex = transfer;
    submissions[transfer].end();
}

void FilterPipeline::applyFilter(std::vector<Texture>& texes,
                                 const fpl::FilterSlot& filterSlot,
                                 SubmissionStack& submissions) const noexcept
{
    int compute = submissions.get();
    const auto& filter = m_layout.instances.find(filterSlot.filterName)->second;
    submissions[compute].begin();

    vkCmdBindPipeline(submissions[compute].commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, filter->pipeline);

    for (int i = 0; i < filterSlot.inputs.size(); ++i)
    {
        int index = filterSlot.inputs[i];
        Texture& tex = index == -1 ? texes[0] : texes[m_layout.slots[index].outputTexture];
        vkCmdBindDescriptorSets(submissions[compute].commandBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                filter->pipelineLayout,
                                i,
                                1,
                                &tex.descriptorSet,
                                0,
                                nullptr);
        submissions[compute].prereqs.push_back(tex.lastSubmissionIndex);
        tex.lastSubmissionIndex = compute;
    }
    vkCmdBindDescriptorSets(submissions[compute].commandBuffer,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            filter->pipelineLayout,
                            filterSlot.inputs.size(),
                            1,
                            &texes[filterSlot.outputTexture].descriptorSet,
                            0,
                            nullptr);
    submissions[compute].prereqs.push_back(texes[filterSlot.outputTexture].lastSubmissionIndex);
    texes[filterSlot.outputTexture].lastSubmissionIndex = compute;

    if (filter->params.size() > 0)
    {
        vkCmdPushConstants(submissions[compute].commandBuffer,
                           filter->pipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT,
                           0,
                           filterSlot.params.size() * sizeof(uint32_t),
                           filterSlot.params.data());
    }

    vkCmdDispatch(submissions[compute].commandBuffer, imageMetadata.groupX, imageMetadata.groupY, 1);

    submissions[compute].end();
}

static void transitionToTransferFromCompute(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    submissions[transfer].begin(VK_PIPELINE_STAGE_TRANSFER_BIT);

    auto barrier = createImageMemoryBarrier();
    barrier.image = tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(
        submissions[transfer].commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    submissions[transfer].prereqs.push_back(tex.lastSubmissionIndex);
    tex.lastSubmissionIndex = transfer;
    submissions[transfer].end();
}

static void copyImageToBuffer(Texture& tex, SubmissionStack& submissions) noexcept
{
    int copyImage = submissions.get();
    submissions[copyImage].begin();

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

    vkCmdCopyImageToBuffer(submissions[copyImage].commandBuffer,
                           tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           stagingBuffer.buffer,
                           1,
                           &region);

    submissions[copyImage].prereqs.push_back(tex.lastSubmissionIndex);
    tex.lastSubmissionIndex = copyImage;
    submissions[copyImage].end();
}

void FilterPipeline::record(std::vector<Texture>& texes, SubmissionStack& submissions) const noexcept
{
    assert(texes.size() == m_layout.texCount);
    transitionToTransfer(texes[0], submissions);
    copyImage(texes[0], submissions);
    transitionToComputeFromTransfer(texes[0], submissions);

    for (int i = 1; i < texes.size(); ++i)
    {
        transitionToComputeFromUndefined(texes[i], submissions);
    }

    for (int i = 0; i < m_layout.slots.size(); ++i)
    {
        applyFilter(texes, m_layout.slots[i], submissions);
    }

    Texture& tex = texes[m_layout.slots.back().outputTexture];
    transitionToTransferFromCompute(tex, submissions);
    copyImageToBuffer(tex, submissions);
}
} // namespace flint
#include "FilterPipeline.h"

#include "FilterInstance.h"
#include "FilterUtils.h"
#include "StagingBuffer.h"
#include "SubmissionInfo.h"
#include "SubmissionStack.h"
#include "Texture.h"
#include "VkContext.h"
#include "cmdparser.hpp"

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace flint
{
FilterPipeline::FilterPipeline(const cli::Parser& args) noexcept : m_layout(args)
{
    if (!m_layout.valid())
    {
        cleanup();
        return;
    }

    for (const auto& slot : m_layout.slots)
    {
        const auto& it = m_filterInstances.find(slot.type);
        if (it == m_filterInstances.end())
        {
            m_filterInstances[slot.type] = std::make_unique<FilterInstance>(slot.type);
            if (!m_filterInstances[slot.type]->valid())
            {
                cleanup();
                return;
            }
        }
    }
    m_valid = true;
}

void FilterPipeline::cleanup() noexcept
{
    m_valid = false;
    for (auto& instance : m_filterInstances)
    {
        instance.second->cleanup();
    }
    for (auto& slot : m_layout.slots)
    {
        if (slot.firstIteration && slot.params.data)
        {
            delete[] static_cast<uint32_t*>(slot.params.data);
        }
    }
}

static bool transitionToTransfer(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    if (!submissions[transfer].begin(VK_PIPELINE_STAGE_TRANSFER_BIT))
    {
        return false;
    }

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
    return submissions[transfer].end();
}

static bool copyImage(Texture& tex, SubmissionStack& submissions) noexcept
{
    int copyImage = submissions.get();
    if (!submissions[copyImage].begin())
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

    vkCmdCopyBufferToImage(submissions[copyImage].commandBuffer,
                           stagingBuffer.buffer,
                           tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);

    submissions[copyImage].prereqs.push_back(tex.lastSubmissionIndex);
    tex.lastSubmissionIndex = copyImage;
    return submissions[copyImage].end();
}

static bool transitionToComputeFromTransfer(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    if (!submissions[transfer].begin())
    {
        return false;
    }

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
    return submissions[transfer].end();
}

static bool transitionToComputeFromUndefined(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    if (!submissions[transfer].begin())
    {
        return false;
    }

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
    return submissions[transfer].end();
}

bool FilterPipeline::applyFilter(std::vector<Texture>& texes,
                                 const fpl::FilterSlot& filterSlot,
                                 SubmissionStack& submissions) const noexcept
{
    int compute = submissions.get();
    FilterInstance* filter = m_filterInstances[filterSlot.type].get();
    if (!(submissions[compute].begin()))
    {
        return false;
    }

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

    if (utils::parameterCount(filterSlot.type))
    {
        vkCmdPushConstants(submissions[compute].commandBuffer,
                           filter->pipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT,
                           0,
                           filterSlot.params.size,
                           filterSlot.params.data);
    }

    vkCmdDispatch(submissions[compute].commandBuffer, imageMetadata.groupX, imageMetadata.groupY, 1);

    return submissions[compute].end();
}

static bool transitionToTransferFromCompute(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    if (!submissions[transfer].begin(VK_PIPELINE_STAGE_TRANSFER_BIT))
    {
        return false;
    }

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
    return submissions[transfer].end();
}

static bool copyImageToBuffer(Texture& tex, SubmissionStack& submissions) noexcept
{
    int copyImage = submissions.get();
    if (!submissions[copyImage].begin())
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

    vkCmdCopyImageToBuffer(submissions[copyImage].commandBuffer,
                           tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           stagingBuffer.buffer,
                           1,
                           &region);

    submissions[copyImage].prereqs.push_back(tex.lastSubmissionIndex);
    tex.lastSubmissionIndex = copyImage;
    return submissions[copyImage].end();
}

bool FilterPipeline::record(std::vector<Texture>& texes, SubmissionStack& submissions) const noexcept
{
    assert(texes.size() == m_layout.texCount);
    if (!(transitionToTransfer(texes[0], submissions) &&
          copyImage(texes[0], submissions) &&
          transitionToComputeFromTransfer(texes[0], submissions)))
    {
        return false;
    }

    for (int i = 1; i < texes.size(); ++i)
    {
        if (!transitionToComputeFromUndefined(texes[i], submissions))
        {
            return false;
        }
    }

    for (int i = 0; i < m_layout.slots.size(); ++i)
    {
        if (!applyFilter(texes, m_layout.slots[i], submissions))
        {
            return false;
        }
    }

    Texture& tex = texes[m_layout.slots.back().outputTexture];
    return transitionToTransferFromCompute(tex, submissions) && copyImageToBuffer(tex, submissions);
}
} // namespace flint
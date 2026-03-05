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
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace flint
{
static std::string toPath(const std::string& filterName)
{
    return "/home/victordadaciu/workspace/flint/compute/" + filterName + ".comp.spv";
}

FilterPipeline::FilterPipeline(const cli::Parser& args) noexcept
{
    m_uniqueTexturesCount = 2;

    std::string filterName = args.get<std::string>("f");

    FilterSlot slot{};
    slot.outputTexture = 1;
    slot.type = toFilterType.find(filterName)->second;
    m_filterSlots.push_back(slot);

    m_filterInstances[slot.type] = std::make_unique<FilterInstance>(toPath(filterName));

    for (const auto& instance : m_filterInstances)
    {
        if (!instance.second->valid())
        {
            m_valid = false;
        }
    }
    m_valid = true;
}

void FilterPipeline::cleanup() noexcept
{
    for (auto& instance : m_filterInstances)
    {
        instance.second->cleanup();
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
                                 const FilterSlot& filterSlot,
                                 SubmissionStack& submissions) const noexcept
{
    int compute = submissions.get();
    FilterInstance* filter = m_filterInstances[filterSlot.type].get();
    if (!(submissions[compute].begin()))
    {
        return false;
    }

    vkCmdBindPipeline(submissions[compute].commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, filter->pipeline);

    // top of tree, input to this filter is the actual unfiltered image
    if (filterSlot.inputFilterSlots.size() == 0)
    {
        vkCmdBindDescriptorSets(submissions[compute].commandBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                filter->pipelineLayout,
                                0,
                                1,
                                &texes[0].descriptorSet,
                                0,
                                nullptr);
        vkCmdBindDescriptorSets(submissions[compute].commandBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                filter->pipelineLayout,
                                1,
                                1,
                                &texes[filterSlot.outputTexture].descriptorSet,
                                0,
                                nullptr);
        submissions[compute].prereqs.push_back(texes[0].lastSubmissionIndex);
        texes[0].lastSubmissionIndex = compute;
        submissions[compute].prereqs.push_back(texes[filterSlot.outputTexture].lastSubmissionIndex);
        texes[filterSlot.outputTexture].lastSubmissionIndex = compute;
    }
    else
    {
        for (int i = 0; i < filterSlot.inputFilterSlots.size(); ++i)
        {
            Texture& tex = texes[m_filterSlots[filterSlot.inputFilterSlots[i]].outputTexture];
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
                                filterSlot.inputFilterSlots.size(),
                                1,
                                &texes[filterSlot.outputTexture].descriptorSet,
                                0,
                                nullptr);
        submissions[compute].prereqs.push_back(texes[filterSlot.outputTexture].lastSubmissionIndex);
        texes[filterSlot.outputTexture].lastSubmissionIndex = compute;
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
    assert(texes.size() >= m_uniqueTexturesCount);
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

    for (int i = 0; i < m_filterSlots.size(); ++i)
    {
        if (!applyFilter(texes, m_filterSlots[i], submissions))
        {
            return false;
        }
    }

    Texture& tex = texes[m_filterSlots[m_filterSlots.size() - 1].outputTexture];
    return transitionToTransferFromCompute(tex, submissions) && copyImageToBuffer(tex, submissions);
}
} // namespace flint
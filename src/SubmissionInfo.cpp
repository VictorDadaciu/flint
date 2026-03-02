#include "SubmissionInfo.h"

#include "VkContext.h"

#include <iostream>
#include <vulkan/vulkan_core.h>

namespace flint
{
void SubmissionInfo::cleanup() noexcept
{
    vkDestroySemaphore(ctx->device, signal, nullptr);
}

bool SubmissionInfo::begin(VkPipelineStageFlags flags) noexcept
{
    stage = flags;

    VkSemaphoreTypeCreateInfo timelineCreateInfo{};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = 0;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = &timelineCreateInfo;

    if (VK_FAILED(vkCreateSemaphore(ctx->device, &semaphoreInfo, nullptr, &signal)))
    {
        std::cout << "Failed to create vulkan semaphore\n";
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = ctx->commandPool;
    allocInfo.commandBufferCount = 1;

    if (VK_FAILED(vkAllocateCommandBuffers(ctx->device, &allocInfo, &commandBuffer)))
    {
        std::cout << "Failed to allocate vulkan command buffer\n";
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (VK_FAILED(vkBeginCommandBuffer(commandBuffer, &beginInfo)))
    {
        std::cout << "Failed to begin vulkan command buffer\n";
        return false;
    }
    return true;
}

bool SubmissionInfo::end() noexcept
{
    if (VK_FAILED(vkEndCommandBuffer(commandBuffer)))
    {
        std::cout << "Failed to end vulkan command buffer\n";
        return false;
    }
    return true;
}
} // namespace flint
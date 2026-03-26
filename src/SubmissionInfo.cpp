#include "SubmissionInfo.h"

#include "Utils.h"
#include "VkContext.h"

#include <vulkan/vulkan_core.h>

namespace flint
{
SubmissionInfo::~SubmissionInfo() noexcept
{
    vkDestroySemaphore(ctx->device, signal, nullptr);
}

void SubmissionInfo::begin(VkPipelineStageFlags flags) noexcept
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
        fail("Failed to create vulkan command buffer");
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = ctx->commandPool;
    allocInfo.commandBufferCount = 1;

    if (VK_FAILED(vkAllocateCommandBuffers(ctx->device, &allocInfo, &commandBuffer)))
    {
        fail("Failed to allocate vulkan command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (VK_FAILED(vkBeginCommandBuffer(commandBuffer, &beginInfo)))
    {
        fail("Failed to create begin command buffer");
    }
}

void SubmissionInfo::end() noexcept
{
    if (VK_FAILED(vkEndCommandBuffer(commandBuffer)))
    {
        fail("Failed to end vulkan command buffer");
    }
}
} // namespace flint
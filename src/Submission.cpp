#include "Submission.h"

#include "VkContext.h"

#include <iostream>

namespace flint::vulkan
{
bool Submission::begin() noexcept
{
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

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (VK_FAILED(vkCreateSemaphore(ctx->device, &semaphoreInfo, nullptr, &semaphore)))
    {
        std::cout << "Failed to create vulkan semaphore\n";
        return false;
    }

    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &commandBuffer;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &semaphore;

    return true;
}

bool Submission::end() noexcept
{
    info.pWaitSemaphores = waitSemaphores.data();
    info.pWaitDstStageMask = waitFlags.data();
    info.waitSemaphoreCount = waitSemaphores.size();

    if (VK_FAILED(vkEndCommandBuffer(commandBuffer)))
    {
        std::cout << "Failed to end vulkan command buffer\n";
        return false;
    }
    return true;
}
} // namespace flint::vulkan
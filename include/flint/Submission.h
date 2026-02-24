#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace flint::vulkan
{
struct Submission
{
    VkSemaphore semaphore{};
    VkCommandBuffer commandBuffer{};
    VkSubmitInfo info{};
    std::vector<VkSemaphore> waitSemaphores{};
    std::vector<VkPipelineStageFlags> waitFlags{};

    bool begin() noexcept;
    bool end() noexcept;
};
} // namespace flint::vulkan
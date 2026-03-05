#pragma once

#include "cmdparser.hpp"

#include <memory>
#include <vulkan/vulkan.h>

namespace flint
{
#define VK_FAILED(x) (x != VK_SUCCESS)

struct VkContext
{
    VkInstance instance{};
    VkPhysicalDevice gpu{};
    VkDevice device{};
    uint8_t queueFamilyIndex{};
    VkQueue queue{};
    VkCommandPool commandPool{};
    VkCommandBuffer commandBuffer{};
    VkDescriptorPool descriptorPool{};
    VkDescriptorSetLayout descriptorSetLayout{};
};

bool init(const cli::Parser&) noexcept;
void cleanup() noexcept;

bool findMemoryType(uint32_t, VkMemoryPropertyFlags, int&) noexcept;

VkImageMemoryBarrier createImageMemoryBarrier() noexcept;

extern std::unique_ptr<VkContext> ctx;
} // namespace flint
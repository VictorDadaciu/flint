#pragma once

#include "Args.h"

#include <memory>
#include <vulkan/vulkan.h>

namespace flint::vulkan
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
    VkDescriptorPool descriptorPool{};
    VkDescriptorSetLayout descriptorSetLayout{};
};

bool init(const Args&) noexcept;
void cleanup() noexcept;

extern std::unique_ptr<VkContext> ctx;
} // namespace flint::vulkan
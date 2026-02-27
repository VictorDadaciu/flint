#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace flint::vulkan
{
constexpr int LOCAL_WORK_SIZE = 32;

extern struct ImageMetadata
{
    int width{};
    int height{};
    int groupX{};
    int groupY{};

    inline int resolution() const noexcept { return width * height; }

    inline int size() const noexcept { return resolution() * 4; }
} imageMetadata;

struct Texture
{
    VkImage image{};
    VkDeviceMemory memory{};
    VkImageView imageView{};
    VkDescriptorSet descriptorSet{};

    void cleanup() noexcept;
};
} // namespace flint::vulkan
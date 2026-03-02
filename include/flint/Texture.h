#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace flint
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

unsigned char* loadImage(const char*) noexcept;

void writeImage(const char*) noexcept;

struct Texture
{
    Texture() noexcept;

    inline bool valid() const noexcept { return m_valid; }

    void cleanup() noexcept;

    VkImage image{};
    VkImageView imageView{};
    VkDeviceMemory memory{};
    VkDescriptorSet descriptorSet{};
    int lastSubmissionIndex = -1;

private:
    bool createImage() noexcept;

    bool createImageView() noexcept;

    bool createDescriptorSet() noexcept;

    bool m_valid{};
};
} // namespace flint
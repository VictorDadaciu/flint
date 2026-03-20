#pragma once

#include <filesystem>
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

unsigned char* loadImage(const std::filesystem::path&) noexcept;

void writeImage(const std::filesystem::path&) noexcept;

struct Texture final
{
    Texture() noexcept;

    Texture(Texture&) = delete;

    Texture(Texture&&) noexcept;

    void operator=(Texture&) = delete;

    void operator=(Texture&&) noexcept;

    ~Texture() noexcept;

    VkImage image{};
    VkImageView imageView{};
    VkDeviceMemory memory{};
    VkDescriptorSet descriptorSet{};
    int lastSubmissionIndex = -1;

private:
    void createImage() noexcept;

    void createImageView() noexcept;

    void createDescriptorSet() noexcept;
};
} // namespace flint
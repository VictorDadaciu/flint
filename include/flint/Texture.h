#pragma once

#include <vulkan/vulkan.h>

namespace flint
{
struct RawImage
{
    const char* filepath = nullptr;
    unsigned char* raw = nullptr;
    int width = -1;
    int height = -1;
    int channels = -1;

    RawImage() = default;

    RawImage(const char* path);

    RawImage(RawImage& other) noexcept;

    RawImage(RawImage&& other) noexcept;

    void operator=(RawImage& other) noexcept;

    void operator=(RawImage&& other) noexcept;

    inline bool valid() const noexcept { return raw != nullptr; }

    ~RawImage();
};

struct Texture
{
    VkImage image{};
    VkDeviceMemory memory{};
    VkImageView imageView{};
    VkDescriptorSet descriptorSet{};

    Texture() = default;

    Texture(Texture&) noexcept;

    Texture(Texture&&) noexcept;

    void operator=(Texture&) noexcept;

    void operator=(Texture&&) noexcept;

    void cleanup() noexcept;

    ~Texture() = default;
};

void createTextureOnDevice(const RawImage&, Texture&) noexcept;
void createTextureOnHostAndCopyToDevice(const RawImage&, Texture&) noexcept;
} // namespace flint
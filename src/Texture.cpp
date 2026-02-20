#include "Texture.h"

#include "VkContext.h"

#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace flint
{
RawImage::RawImage(const char* path) : filepath(path)
{
    raw = stbi_load(filepath, &width, &height, &channels, STBI_rgb);
}

RawImage::RawImage(RawImage& other) noexcept :
    filepath(other.filepath), raw(other.raw), width(other.width), height(other.height), channels(other.channels)
{
    other.raw = nullptr;
}

RawImage::RawImage(RawImage&& other) noexcept :
    filepath(other.filepath), raw(other.raw), width(other.width), height(other.height), channels(other.channels)
{
    other.raw = nullptr;
}

void RawImage::operator=(RawImage& other) noexcept
{
    filepath = other.filepath;
    raw = other.raw;
    width = other.width;
    height = other.height;
    channels = other.channels;
    other.raw = nullptr;
}

void RawImage::operator=(RawImage&& other) noexcept
{
    filepath = other.filepath;
    raw = other.raw;
    width = other.width;
    height = other.height;
    channels = other.channels;
    other.raw = nullptr;
}

RawImage::~RawImage()
{
    stbi_image_free(raw);
}

Texture::Texture(Texture& other) noexcept : image(other.image), memory(other.memory), imageView(other.imageView)
{
    other.image = nullptr;
    other.memory = nullptr;
    other.imageView = nullptr;
}

Texture::Texture(Texture&& other) noexcept : image(other.image), memory(other.memory), imageView(other.imageView)
{
    other.image = nullptr;
    other.memory = nullptr;
    other.imageView = nullptr;
}

void Texture::operator=(Texture& other) noexcept
{
    image = other.image;
    memory = other.memory;
    imageView = other.imageView;
    other.image = nullptr;
    other.memory = nullptr;
    other.imageView = nullptr;
}

void Texture::cleanup() noexcept
{
    vkDestroyImageView(vulkan::ctx->device, imageView, nullptr);
    vkDestroyImage(vulkan::ctx->device, image, nullptr);
    vkFreeMemory(vulkan::ctx->device, memory, nullptr);
}

void createTextureOnDevice(const RawImage& raw, Texture& tex) noexcept
{
}

void createTextureOnHostAndCopyToDevice(const RawImage& raw, Texture& tex) noexcept
{
}
} // namespace flint
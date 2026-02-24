#include "Texture.h"

#include "VkContext.h"

#include <vulkan/vulkan_core.h>

namespace flint::vulkan
{
ImageMetadata imageMetadata{};

void Texture::cleanup() noexcept
{
    vkDestroyImageView(ctx->device, imageView, nullptr);
    vkDestroyImage(ctx->device, image, nullptr);
    vkFreeMemory(ctx->device, memory, nullptr);
}
} // namespace flint::vulkan
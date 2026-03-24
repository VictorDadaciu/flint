#include "Texture.h"

#include "Error.h"
#include "StagingBuffer.h"
#include "VkContext.h"

#include <filesystem>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <vulkan/vulkan_core.h>

namespace flint
{
ImageMetadata imageMetadata{};

unsigned char* loadImage(const std::filesystem::path& path) noexcept
{
    int channels{};
    unsigned char* raw =
        stbi_load(path.c_str(), &imageMetadata.width, &imageMetadata.height, &channels, STBI_rgb_alpha);
    if (!raw)
    {
        fail("Failed to load image at path '" + path.string() + "': " + stbi_failure_reason());
    }
    imageMetadata.groupX =
        imageMetadata.width / LOCAL_WORK_SIZE + 1 * (int)((bool)(imageMetadata.width % LOCAL_WORK_SIZE));
    imageMetadata.groupY =
        imageMetadata.height / LOCAL_WORK_SIZE + 1 * (int)((bool)(imageMetadata.height % LOCAL_WORK_SIZE));
    return raw;
}

void writeImage(const std::filesystem::path& path) noexcept
{
    std::cout << "Outputting final image to " << path << "\n";
    unsigned char* raw = stagingBuffer.getAsRawImage();
    stbi_write_jpg(path.c_str(), imageMetadata.width, imageMetadata.height, 4, raw, 100);
    stbi_image_free(raw);
}

void Texture::createImage() noexcept
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = imageMetadata.width;
    imageInfo.extent.height = imageMetadata.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UINT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (VK_FAILED(vkCreateImage(ctx->device, &imageInfo, nullptr, &image)))
    {
        fail("Failed to create vulkan image");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(ctx->device, image, &memRequirements);

    int memoryTypeIndex{};
    if (!findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryTypeIndex))
    {
        fail("Failed to find suitable memory for vulkan image");
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (VK_FAILED(vkAllocateMemory(ctx->device, &allocInfo, nullptr, &memory)))
    {
        fail("Failed to allocate memory on device for vulkan image");
    }

    vkBindImageMemory(ctx->device, image, memory, 0);
}

void Texture::createImageView() noexcept
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UINT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (VK_FAILED(vkCreateImageView(ctx->device, &viewInfo, nullptr, &imageView)))
    {
        fail("Failed to create vulkan image view");
    }
}

void Texture::createDescriptorSet() noexcept
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = ctx->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &ctx->descriptorSetLayout;

    if (VK_FAILED(vkAllocateDescriptorSets(ctx->device, &allocInfo, &descriptorSet)))
    {
        fail("Failed to allocate descriptor set");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = imageView;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.dstArrayElement = 0;
    write.dstBinding = 0;
    write.pImageInfo = &imageInfo;
    write.dstSet = descriptorSet;

    vkUpdateDescriptorSets(ctx->device, 1, &write, 0, nullptr);
}

Texture::Texture(Texture&& other) noexcept :
    image(other.image), imageView(other.imageView), memory(other.memory), descriptorSet(other.descriptorSet),
    lastSubmissionIndex(other.lastSubmissionIndex)
{
    other.image = nullptr;
    other.imageView = nullptr;
    other.memory = nullptr;
    other.descriptorSet = nullptr;
    other.lastSubmissionIndex = -1;
}

void Texture::operator=(Texture&& other) noexcept
{
    image = other.image;
    imageView = other.imageView;
    memory = other.memory;
    descriptorSet = other.descriptorSet;
    lastSubmissionIndex = other.lastSubmissionIndex;
    other.image = nullptr;
    other.imageView = nullptr;
    other.memory = nullptr;
    other.descriptorSet = nullptr;
    other.lastSubmissionIndex = -1;
}

Texture::Texture() noexcept :
    image(nullptr), imageView(nullptr), memory(nullptr), descriptorSet(nullptr), lastSubmissionIndex(-1)
{
    createImage();
    createImageView();
    createDescriptorSet();
}

Texture::~Texture() noexcept
{
    vkDestroyImageView(ctx->device, imageView, nullptr);
    vkDestroyImage(ctx->device, image, nullptr);
    vkFreeMemory(ctx->device, memory, nullptr);
}
} // namespace flint
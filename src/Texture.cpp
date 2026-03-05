#include "Texture.h"

#include "StagingBuffer.h"
#include "VkContext.h"

#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <vulkan/vulkan_core.h>

namespace flint
{
ImageMetadata imageMetadata{};

unsigned char* loadImage(const std::string& path) noexcept
{
    int channels{};
    unsigned char* raw = stbi_load(path.c_str(), &imageMetadata.width, &imageMetadata.height, &channels, STBI_rgb_alpha);
    if (!raw)
    {
        std::cout << "Failed to load image at path " << path << ": " << stbi_failure_reason() << "\n";
        return nullptr;
    }
    imageMetadata.groupX =
        imageMetadata.width / LOCAL_WORK_SIZE + 1 * (int)((bool)(imageMetadata.width % LOCAL_WORK_SIZE));
    imageMetadata.groupY =
        imageMetadata.height / LOCAL_WORK_SIZE + 1 * (int)((bool)(imageMetadata.height % LOCAL_WORK_SIZE));

    return raw;
}

void writeImage(const std::string& path) noexcept
{
    unsigned char* raw = stagingBuffer.getAsRawImage();
    stbi_write_jpg(path.c_str(), imageMetadata.width, imageMetadata.height, 4, raw, 100);
    stbi_image_free(raw);
}

bool Texture::createImage() noexcept
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
        std::cout << "Failed to create vulkan image\n";
        return false;
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(ctx->device, image, &memRequirements);

    int memoryTypeIndex{};
    if (!findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryTypeIndex))
    {
        std::cout << "Failed to find suitable memory for vulkan image\n";
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (VK_FAILED(vkAllocateMemory(ctx->device, &allocInfo, nullptr, &memory)))
    {
        std::cout << "Failed to allocate memory on device for vulkan image\n";
        return false;
    }

    vkBindImageMemory(ctx->device, image, memory, 0);
    return true;
}

bool Texture::createImageView() noexcept
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
        std::cout << "Failed to create vulkan image view\n";
        return false;
    }
    return true;
}

bool Texture::createDescriptorSet() noexcept
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = ctx->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &ctx->descriptorSetLayout;

    if (VK_FAILED(vkAllocateDescriptorSets(ctx->device, &allocInfo, &descriptorSet)))
    {
        std::cout << "Failed to allocate descriptor set\n";
        return false;
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
    return true;
}

Texture::Texture() noexcept
{
    m_valid = createImage() && createImageView() && createDescriptorSet();
}

void Texture::cleanup() noexcept
{
    vkDestroyImageView(ctx->device, imageView, nullptr);
    vkDestroyImage(ctx->device, image, nullptr);
    vkFreeMemory(ctx->device, memory, nullptr);
}
} // namespace flint
#include "StagingBuffer.h"

#include "FilterUtils.h"
#include "Texture.h"
#include "VkContext.h"

#include <cstring>
#include <emmintrin.h>
#include <iostream>
#include <stb_image.h>
#include <vulkan/vulkan_core.h>

namespace flint::vulkan
{
StagingBuffer stagingBuffer{};

void StagingBuffer::cleanup() noexcept
{
    vkDestroyBuffer(ctx->device, m_buffer, nullptr);
    vkFreeMemory(ctx->device, m_memory, nullptr);
}

bool StagingBuffer::createFromRawImage(unsigned char* raw)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageMetadata.size();
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (VK_FAILED(vkCreateBuffer(ctx->device, &bufferInfo, nullptr, &m_buffer)))
    {
        std::cout << "Failed to create vulkan buffer\n";
        stbi_image_free(raw);
        cleanup();
        return false;
    }

    VkMemoryRequirements memRequirements{};
    vkGetBufferMemoryRequirements(ctx->device, m_buffer, &memRequirements);
    int memoryTypeIndex{};
    if (!findMemoryType(memRequirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        memoryTypeIndex))
    {
        std::cout << "Failed to find suitable memory for vulkan buffer\n";
        stbi_image_free(raw);
        cleanup();
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (VK_FAILED(vkAllocateMemory(ctx->device, &allocInfo, nullptr, &m_memory)))
    {
        std::cout << "Failed to allocate memory on device for vulkan buffer\n";
        stbi_image_free(raw);
        cleanup();
        return false;
    }

    vkBindBufferMemory(ctx->device, m_buffer, m_memory, 0);

    void* data{};
    vkMapMemory(ctx->device, m_memory, 0, imageMetadata.size(), 0, &data);
    memcpy(data, raw, imageMetadata.size());
    vkUnmapMemory(ctx->device, m_memory);
    stbi_image_free(raw);
    return true;
}

unsigned char* StagingBuffer::getAsRawImage() const noexcept
{
    unsigned char* ret = new unsigned char[imageMetadata.size()];
    void* data{};
    vkMapMemory(ctx->device, m_memory, 0, imageMetadata.size(), 0, &data);
    memcpy(ret, data, imageMetadata.size());
    vkUnmapMemory(ctx->device, m_memory);
    return ret;
}
} // namespace flint::vulkan
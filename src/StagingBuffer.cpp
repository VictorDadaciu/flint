#include "StagingBuffer.h"

#include "Texture.h"
#include "Utils.h"
#include "VkContext.h"

#include <cstring>
#include <emmintrin.h>
#include <stb_image.h>
#include <vulkan/vulkan_core.h>

namespace flint
{
StagingBuffer stagingBuffer{};

void StagingBuffer::cleanup() noexcept
{
    vkDestroyBuffer(ctx->device, buffer, nullptr);
    vkFreeMemory(ctx->device, memory, nullptr);
}

void StagingBuffer::createFromRawImage(unsigned char* raw)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageMetadata.size();
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (VK_FAILED(vkCreateBuffer(ctx->device, &bufferInfo, nullptr, &buffer)))
    {
        fail("Failed to create vulkan buffer");
    }

    VkMemoryRequirements memRequirements{};
    vkGetBufferMemoryRequirements(ctx->device, buffer, &memRequirements);
    int memoryTypeIndex{};
    if (!findMemoryType(memRequirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        memoryTypeIndex))
    {
        fail("Failed to find suitable memory for vulkan buffer");
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (VK_FAILED(vkAllocateMemory(ctx->device, &allocInfo, nullptr, &memory)))
    {
        fail("Failed to allocate memory on device for vulkan buffer");
    }

    vkBindBufferMemory(ctx->device, buffer, memory, 0);

    void* data{};
    vkMapMemory(ctx->device, memory, 0, imageMetadata.size(), 0, &data);
    memcpy(data, raw, imageMetadata.size());
    vkUnmapMemory(ctx->device, memory);
    stbi_image_free(raw);
}

unsigned char* StagingBuffer::getAsRawImage() const noexcept
{
    unsigned char* ret = new unsigned char[imageMetadata.size()];
    void* data{};
    vkMapMemory(ctx->device, memory, 0, imageMetadata.size(), 0, &data);
    memcpy(ret, data, imageMetadata.size());
    vkUnmapMemory(ctx->device, memory);
    return ret;
}
} // namespace flint
#pragma once

#include <vulkan/vulkan.h>

namespace flint::vulkan
{
extern struct StagingBuffer
{
    StagingBuffer() = default;

    bool createFromRawImage(unsigned char*);

    unsigned char* getAsRawImage() const noexcept;

    void cleanup() noexcept;

    VkBuffer m_buffer{};
    VkDeviceMemory m_memory{};
} stagingBuffer;
} // namespace flint::vulkan
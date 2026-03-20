#pragma once

#include <vulkan/vulkan.h>

namespace flint
{
extern struct StagingBuffer final
{
    StagingBuffer() = default;

    bool createFromRawImage(unsigned char*);

    unsigned char* getAsRawImage() const noexcept;

    void cleanup() noexcept;

    VkBuffer buffer{};
    VkDeviceMemory memory{};
} stagingBuffer;
} // namespace flint
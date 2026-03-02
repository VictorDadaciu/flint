#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace flint
{
struct SubmissionInfo
{
    bool begin(VkPipelineStageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) noexcept;

    bool end() noexcept;

    void cleanup() noexcept;

    VkCommandBuffer commandBuffer{};
    VkSemaphore signal{};
    VkPipelineStageFlags stage{};
    std::vector<int> prereqs{};

    std::vector<uint64_t> waitValues{};
    std::vector<VkSemaphore> waits{};
    std::vector<VkPipelineStageFlags> dstMasks{};
};
} // namespace flint
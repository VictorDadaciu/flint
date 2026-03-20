#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace flint
{
struct SubmissionInfo final
{
    SubmissionInfo() = default;

    SubmissionInfo(SubmissionInfo&) = delete;
    SubmissionInfo(SubmissionInfo&&) = delete;

    void operator=(SubmissionInfo&) = delete;
    void operator=(SubmissionInfo&&) = delete;

    ~SubmissionInfo() noexcept;

    void begin(VkPipelineStageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) noexcept;

    void end() noexcept;

    VkCommandBuffer commandBuffer{};
    VkSemaphore signal{};
    VkPipelineStageFlags stage{};
    std::vector<int> prereqs{};

    std::vector<uint64_t> waitValues{};
    std::vector<VkSemaphore> waits{};
    std::vector<VkPipelineStageFlags> dstMasks{};
};
} // namespace flint
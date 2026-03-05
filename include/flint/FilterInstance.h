#pragma once

#include <string>
#include <vulkan/vulkan_core.h>

namespace flint
{
class FilterInstance
{
public:
    FilterInstance(const std::string&) noexcept;

    void cleanup() noexcept;

    inline bool valid() const noexcept { return m_valid; }

    VkPipeline pipeline{};
    VkPipelineLayout pipelineLayout{};

private:
    bool m_valid{};
};
} // namespace flint
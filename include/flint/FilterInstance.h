#pragma once

#include <string>
#include <vector>
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
    int inputCount = -1;
    std::vector<std::tuple<std::string, bool>> params;

private:
    VkShaderModule compileFromSource(const std::string&) noexcept;

    VkShaderModule tryLoadFromCache(const std::string&) noexcept;

    bool serializeToCache(const std::string&) const noexcept;

    bool deserializeFromCache(const std::string&) noexcept;

    bool m_valid{};
};
} // namespace flint
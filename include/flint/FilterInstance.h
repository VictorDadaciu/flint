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

    FilterInstance(FilterInstance&) = delete;

    FilterInstance(FilterInstance&&) noexcept;

    void operator=(FilterInstance&) = delete;

    void operator=(FilterInstance&&) noexcept;

    ~FilterInstance() noexcept;

    VkPipeline pipeline{};
    VkPipelineLayout pipelineLayout{};
    int inputCount = -1;
    std::vector<std::tuple<std::string, bool>> params;

private:
    VkShaderModule compileFromSource(const std::string&) noexcept;

    VkShaderModule tryLoadFromCache(const std::string&) noexcept;

    bool serializeToCache(const std::string&) const noexcept;

    bool deserializeFromCache(const std::string&) noexcept;
};
} // namespace flint
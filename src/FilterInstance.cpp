#include "FilterInstance.h"

#include "VkContext.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace flint
{
static VkShaderModule createShaderModule(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "Failed to open shader file: " << path << "\n";
        return nullptr;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule{};
    if (VK_FAILED(vkCreateShaderModule(ctx->device, &createInfo, nullptr, &shaderModule)))
    {
        std::cout << "Failed to create vulkan shader module: " << path << "\n";
        return nullptr;
    }

    return shaderModule;
}

void FilterInstance::cleanup() noexcept
{
    vkDestroyPipeline(ctx->device, pipeline, nullptr);
    vkDestroyPipelineLayout(ctx->device, pipelineLayout, nullptr);
}

FilterInstance::FilterInstance(const std::string& path) noexcept
{
    std::vector<VkDescriptorSetLayout> descriptorSets(2, ctx->descriptorSetLayout);
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = descriptorSets.size();
    pipelineLayoutInfo.pSetLayouts = descriptorSets.data();

    if (VK_FAILED(vkCreatePipelineLayout(ctx->device, &pipelineLayoutInfo, nullptr, &pipelineLayout)))
    {
        cleanup();
        return;
    }

    VkShaderModule module = createShaderModule(path);
    VkPipelineShaderStageCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderInfo.module = module;
    shaderInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage = shaderInfo;

    if (VK_FAILED(vkCreateComputePipelines(ctx->device, nullptr, 1, &pipelineInfo, nullptr, &pipeline)))
    {
        vkDestroyShaderModule(ctx->device, module, nullptr);
        cleanup();
        return;
    }

    vkDestroyShaderModule(ctx->device, module, nullptr);
    m_valid = true;
}
} // namespace flint
#pragma once

#include "Submission.h"
#include "Texture.h"
#include "VkContext.h"
#include "stages/FilterStage.h"

#include <array>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace flint::vulkan
{
template<int Inputs>
class FilterOp final : public FilterStage
{
public:
    FilterOp(const char* path, const std::array<FilterStage*, Inputs>& inputs, FilterStage* output) noexcept :
        m_output(output)
    {
        std::vector<VkDescriptorSetLayout> descriptorSets(Inputs + 1, ctx->descriptorSetLayout);
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = descriptorSets.size();
        pipelineLayoutInfo.pSetLayouts = descriptorSets.data();

        if (VK_FAILED(vkCreatePipelineLayout(ctx->device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout)))
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
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.stage = shaderInfo;

        if (VK_FAILED(vkCreateComputePipelines(ctx->device, nullptr, 1, &pipelineInfo, nullptr, &m_pipeline)))
        {
            vkDestroyShaderModule(ctx->device, module, nullptr);
            cleanup();
            return;
        }

        vkDestroyShaderModule(ctx->device, module, nullptr);

        if (!execute(inputs))
        {
            cleanup();
            return;
        }
        m_valid = true;
    }

    inline const Texture& tex() const noexcept override { return m_output->tex(); }

    inline const VkSemaphore& signal() const noexcept override { return m_submission.semaphore; }

    inline std::vector<VkSubmitInfo> submitInfos() const noexcept override { return {m_submission.info}; }

    void cleanup() noexcept override
    {
        m_valid = false;
        vkDestroySemaphore(ctx->device, m_submission.semaphore, nullptr);
        vkDestroyPipelineLayout(ctx->device, m_pipelineLayout, nullptr);
        vkDestroyPipeline(ctx->device, m_pipeline, nullptr);
    }

private:
    bool execute(const std::array<FilterStage*, Inputs>& inputs) noexcept
    {
        if (!(m_submission.begin()))
        {
            cleanup();
            return false;
        }

        vkCmdBindPipeline(m_submission.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

        for (int i = 0; i < Inputs; ++i)
        {
            vkCmdBindDescriptorSets(m_submission.commandBuffer,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    m_pipelineLayout,
                                    i,
                                    1,
                                    &inputs[i]->tex().descriptorSet,
                                    0,
                                    nullptr);
        }
        vkCmdBindDescriptorSets(m_submission.commandBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_pipelineLayout,
                                Inputs,
                                1,
                                &m_output->tex().descriptorSet,
                                0,
                                nullptr);

        vkCmdDispatch(m_submission.commandBuffer, imageMetadata.groupX, imageMetadata.groupY, 1);

        for (FilterStage* input : inputs)
        {
            if (!input->waitedOn())
            {
                m_submission.waitSemaphores.push_back(input->signal());
                m_submission.waitFlags.push_back(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                input->setWaitedOn(true);
            }
        }
        if (!m_output->waitedOn())
        {
            m_submission.waitSemaphores.push_back(m_output->signal());
            m_submission.waitFlags.push_back(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            m_output->setWaitedOn(true);
        }
        return m_submission.end();
    }

    VkShaderModule createShaderModule(const char* path) const
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

    FilterStage* m_output{};
    VkPipeline m_pipeline{};
    VkPipelineLayout m_pipelineLayout{};
    Submission m_submission{};
};
} // namespace flint::vulkan
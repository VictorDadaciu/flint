#pragma once

#include "Submission.h"
#include "Texture.h"
#include "VkContext.h"
#include "stages/FilterStage.h"
#include "stages/GenerateImageStage.h"

#include <algorithm>
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
    FilterOp(const char* path, const std::array<FilterStage*, Inputs>& stages, bool last = false) noexcept :
        m_output(last)
    {
        if (!m_output.valid())
        {
            cleanup();
            return;
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &ctx->descriptorSetLayouts[Inputs - 1];

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

        if (!execute(stages))
        {
            cleanup();
            return;
        }
        m_valid = true;
    }

    inline const VkSemaphore& signal() const noexcept override { return m_submission.semaphore; }

    inline std::vector<VkSubmitInfo> submitInfos() const noexcept override
    {
        std::vector<VkSubmitInfo> infos = m_output.submitInfos();
        std::vector<VkSubmitInfo> ret(Inputs + 1);
        for (int i = 0; i < Inputs; ++i)
        {
            ret[i] = infos[i];
        }
        ret[Inputs] = m_submission.info;
        return ret;
    }

    inline const Texture& tex() const noexcept override { return m_output.tex(); }

    void cleanup() noexcept override
    {
        m_valid = false;
        m_output.cleanup();
        vkDestroySemaphore(ctx->device, m_submission.semaphore, nullptr);
        vkDestroyPipelineLayout(ctx->device, m_pipelineLayout, nullptr);
        vkDestroyPipeline(ctx->device, m_pipeline, nullptr);
    }

private:
    bool execute(const std::array<FilterStage*, Inputs>& stages) noexcept
    {
        VkDescriptorSet descriptorSet{};
        if (!(allocateDescriptors(stages, descriptorSet) && m_submission.begin()))
        {
            cleanup();
            return false;
        }

        vkCmdBindPipeline(m_submission.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
        vkCmdBindDescriptorSets(m_submission.commandBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_pipelineLayout,
                                0,
                                1,
                                &descriptorSet,
                                0,
                                nullptr);

        vkCmdDispatch(m_submission.commandBuffer, imageMetadata.groupX, imageMetadata.groupY, 1);

        m_submission.waitSemaphores.resize(Inputs + 1);
        std::transform(stages.cbegin(),
                       stages.cend(),
                       m_submission.waitSemaphores.begin(),
                       [](FilterStage* stage) -> const VkSemaphore&
                       {
                           return stage->signal();
                       });
        m_submission.waitSemaphores[Inputs] = m_output.signal();
        m_submission.waitFlags.resize(Inputs + 1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        return m_submission.end();
    }

    bool allocateDescriptors(const std::array<FilterStage*, Inputs>& stages, VkDescriptorSet& descriptorSet) noexcept
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = ctx->descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &ctx->descriptorSetLayouts[Inputs - 1];

        if (VK_FAILED(vkAllocateDescriptorSets(ctx->device, &allocInfo, &descriptorSet)))
        {
            std::cout << "Failed to allocate descriptor set\n";
            return false;
        }

        std::array<VkDescriptorImageInfo, Inputs + 1> imageInfos{};
        std::array<VkWriteDescriptorSet, Inputs + 1> descriptorWrites{};
        for (int i = 0; i < descriptorWrites.size() - 1; ++i)
        {
            imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfos[i].imageView = stages[i]->tex().imageView;

            descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[i].dstSet = descriptorSet;
            descriptorWrites[i].dstBinding = i;
            descriptorWrites[i].dstArrayElement = 0;
            descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[i].descriptorCount = 1;
            descriptorWrites[i].pImageInfo = &imageInfos[i];
        }
        imageInfos[Inputs].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfos[Inputs].imageView = m_output.tex().imageView;

        descriptorWrites[Inputs].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[Inputs].dstSet = descriptorSet;
        descriptorWrites[Inputs].dstBinding = Inputs;
        descriptorWrites[Inputs].dstArrayElement = 0;
        descriptorWrites[Inputs].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrites[Inputs].descriptorCount = 1;
        descriptorWrites[Inputs].pImageInfo = &imageInfos[Inputs];

        vkUpdateDescriptorSets(ctx->device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
        return true;
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

    GenerateImageStage m_output{};
    VkPipeline m_pipeline{};
    VkPipelineLayout m_pipelineLayout{};
    Submission m_submission{};
};
} // namespace flint::vulkan
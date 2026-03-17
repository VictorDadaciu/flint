#include "FilterInstance.h"

#include "Utils.h"
#include "VkContext.h"
#include "shaderc/shaderc.h"
#include "shaderc/shaderc.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace flint
{
static constexpr const char* filterPath = "/home/victordadaciu/workspace/flint/filters/";
static constexpr const char* cachePath = "/home/victordadaciu/workspace/flint/filters/.cache/";

static std::string_view nextToken(std::string_view& view, int index = 0) noexcept
{
    for (int i = 0; i < index; ++i)
    {
        view.remove_prefix(view.find_first_not_of(" \n"));
        view.remove_prefix(view.find_first_of(" \n"));
    }
    view.remove_prefix(view.find_first_not_of(" \n"));
    auto whitespaceIndex = view.find_first_of(" \n");
    std::string_view token = view.substr(0, whitespaceIndex);
    view.remove_prefix(whitespaceIndex + 1);
    return token;
}

// TODO better error handling this is a mess
FilterInstance::FilterInstance(const std::string& filterName) noexcept
{
    if (!std::filesystem::exists(filterPath + filterName + ".comp"))
    {
        std::cout << "The filter requested does not exist " << filterName << "\n";
        return;
    }
    VkShaderModule module = tryLoadFromCache(filterName);
    if (!module)
    {
        module = compileFromSource(filterName);
    }
    if (!module)
    {
        return;
    }

    std::vector<VkDescriptorSetLayout> descriptorSets(inputCount + 1, ctx->descriptorSetLayout);
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = descriptorSets.size();
    pipelineLayoutInfo.pSetLayouts = descriptorSets.data();

    VkPushConstantRange range{};
    if (params.size() > 0)
    {
        range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        range.size = params.size() * 4; // only supports uint32_ts and floats, both have a size of 4 bytes

        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &range;
    }

    if (VK_FAILED(vkCreatePipelineLayout(ctx->device, &pipelineLayoutInfo, nullptr, &pipelineLayout)))
    {
        cleanup();
        return;
    }

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

void FilterInstance::cleanup() noexcept
{
    m_valid = false;
    vkDestroyPipeline(ctx->device, pipeline, nullptr);
    vkDestroyPipelineLayout(ctx->device, pipelineLayout, nullptr);
}

VkShaderModule FilterInstance::tryLoadFromCache(const std::string& filterName) noexcept
{
    std::filesystem::path path = filterPath + filterName + ".comp";
    std::filesystem::path spvPath = cachePath + filterName + ".comp.spv";
    if (!std::filesystem::exists(spvPath))
    {
        std::cout << "SPIRV file does not exist for " << filterName << ", needs to be compiled\n";
        return nullptr;
    }
    if (std::filesystem::last_write_time(path) > std::filesystem::last_write_time(spvPath))
    {
        std::cout << "SPIRV file is too old, " << filterName << " needs to be recompiled\n";
        return nullptr;
    }

    if (!deserializeFromCache(filterName))
    {
        return nullptr;
    }

    const auto buffer = utils::readEntireFile(spvPath);
    if (buffer.empty())
    {
        return nullptr;
    }
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule{};
    if (VK_FAILED(vkCreateShaderModule(ctx->device, &createInfo, nullptr, &shaderModule)))
    {
        std::cout << "Failed to create vulkan shader module for " << filterName << "\n";
        return nullptr;
    }
    return shaderModule;
}

VkShaderModule FilterInstance::compileFromSource(const std::string& filterName) noexcept
{
    std::cout << "Compiling " << filterName << "\n";
    std::filesystem::create_directory(cachePath);
    shaderc::Compiler compiler{};
    std::string src;
    {
        auto fileVec = utils::readEntireFile(filterPath + filterName + ".comp");
        if (fileVec.empty())
        {
            return nullptr;
        }
        src = std::string{fileVec.cbegin(), fileVec.cend()};
    }

    // to assembly for reflection
    std::string assembly;
    {
        shaderc::CompileOptions options{};
        options.SetOptimizationLevel(shaderc_optimization_level_zero);
        shaderc::AssemblyCompilationResult result = compiler.CompileGlslToSpvAssembly(
            src.c_str(), shaderc_shader_kind::shaderc_glsl_compute_shader, filterName.c_str(), options);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            std::cout << result.GetErrorMessage();
            return nullptr;
        }
        assembly = std::string{result.cbegin(), result.cend()};
    }

    std::string_view view{assembly};
    while (!view.empty())
    {
        auto token = nextToken(view);
        if (token == "OpMemberName")
        {
            if (nextToken(view) == "%Params")
            {
                auto paramName = nextToken(view, 1);
                params.push_back(std::tuple<std::string, bool>{paramName.substr(1, paramName.size() - 2), false});
            }
        }
        else if (token == "%Params")
        {
            if (nextToken(view) == "=" && nextToken(view) == "OpTypeStruct")
            {
                token = nextToken(view);
                size_t index{};
                while (token == "%int" || token == "%float")
                {
                    if (token == "%float")
                    {
                        std::get<1>(params[index]) = true;
                    }
                    ++index;
                    token = nextToken(view);
                }
            }
        }
        else if (token == "DescriptorSet")
        {
            ++inputCount;
        }
    }
    serializeToCache(filterName);

    // to binary
    std::vector<uint32_t> buffer{};
    {
        shaderc::CompileOptions options{};
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
            src.c_str(), shaderc_shader_kind::shaderc_glsl_compute_shader, filterName.c_str(), options);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            std::cout << result.GetErrorMessage();
            return nullptr;
        }
        buffer = std::vector<uint32_t>{result.cbegin(), result.cend()};
    }
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    VkShaderModule shaderModule{};
    if (VK_FAILED(vkCreateShaderModule(ctx->device, &createInfo, nullptr, &shaderModule)))
    {
        std::cout << "Failed to create vulkan shader module for " << filterName << "\n";
        return nullptr;
    }
    std::filesystem::path path{cachePath + filterName + ".comp.spv"};
    std::ofstream f{path, std::ios_base::binary};
    if (!f)
    {
        std::cout << "Error opening shader file for write " << path << "\n";
        return nullptr;
    }
    f.write(reinterpret_cast<const char*>(buffer.data()), buffer.size() * sizeof(uint32_t));
    f.close();
    return shaderModule;
}

bool FilterInstance::serializeToCache(const std::string& filterName) const noexcept
{
    std::cout << "Serializing compiled pipeline and caching...\n";
    std::vector<uint32_t> toWrite{};
    toWrite.push_back(inputCount);
    toWrite.push_back(params.size());
    for (const auto& param : params)
    {
        utils::combine(std::get<0>(param), toWrite);
        toWrite.push_back(std::get<1>(param));
    }

    std::filesystem::path path{cachePath + filterName + ".cache"};
    std::ofstream f{path, std::ios_base::binary};
    if (!f)
    {
        std::cout << "Error opening cache file for write " << path << "\n";
        return false;
    }
    f.write(reinterpret_cast<const char*>(toWrite.data()), toWrite.size() * sizeof(uint32_t));
    f.close();

    std::cout << "Caching finished successfully\n";
    return true;
}

bool FilterInstance::deserializeFromCache(const std::string& filterName) noexcept
{
    std::filesystem::path fullCachePath{cachePath + filterName + ".cache"};
    if (!std::filesystem::exists(fullCachePath))
    {
        std::cout << "Cache file for " << filterName << " not found\n";
        return false;
    }

    std::cout << "Found cache file for " << filterName << ", deserializing and loading filter...\n";

    std::ifstream f{fullCachePath, std::ios_base::binary};
    inputCount = utils::readInt(f);
    params.resize(utils::readInt(f));
    for (auto& param : params)
    {
        std::get<0>(param) = utils::uncombine(f);
        std::get<1>(param) = utils::readInt(f);
    }
    f.close();

    std::cout << "Deserializing finished successfully\n";
    return true;
}
} // namespace flint
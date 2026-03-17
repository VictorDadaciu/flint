#include "fpl/PipelineLayout.h"

#include "FilterInstance.h"
#include "Utils.h"
#include "cmdparser.hpp"
#include "fpl/LineParser.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace flint::fpl
{
static constexpr int VERY_BIG = 1000000;
static constexpr const char* cachePath = "/home/victordadaciu/workspace/flint/pipelines/.cache/";

struct TexturePlaceholder final
{
    std::string name{};
    int createdAt = VERY_BIG; // first height, then filter slot index
    int lastUsedAt = -1;
    int tex = -1;
};

static int findTexPlaceholder(const std::string_view& name,
                              const std::vector<TexturePlaceholder>& texPlaceholders) noexcept
{
    for (int i = 0; i < texPlaceholders.size(); ++i)
    {
        if (name == texPlaceholders[i].name)
        {
            return i;
        }
    }
    return -1;
}

PipelineLayout::PipelineLayout(const cli::Parser& parser) noexcept
{
    std::string filter = parser.get<std::string>("f");
    if (!filter.ends_with(".fpl"))
    {
        if (!createFromFilterName(parser))
        {
            m_valid = false;
            return;
        }
    }
    else
    {
        std::filesystem::create_directory(cachePath);
        std::filesystem::path path{filter};
        if (!deserializeFromCache(path) && !loadFplFromSource(path))
        {
            m_valid = false;
            return;
        }
    }
    m_valid = true;
}

void PipelineLayout::cleanup() noexcept
{
    for (auto& instance : instances)
    {
        instance.second->cleanup();
    }
}

bool PipelineLayout::createFromFilterName(const cli::Parser& parser) noexcept
{
    texCount = 2;

    FilterSlot slot{};
    slot.inputs = {-1};
    slot.outputTexture = 1;
    slot.filterName = parser.get<std::string>("f");

    auto instance = std::make_unique<FilterInstance>(slot.filterName);
    if (!instance->valid())
    {
        instance->cleanup();
        return false;
    }

    if (instance->inputCount > 1)
    {
        std::cout << "Invalid filter call, cannot call a filter that needs more than one input from the command-line\n";
        cleanup();
        return false;
    }

    int i = 0;
    slot.params.resize(instance->params.size());
    for (const auto& param : instance->params)
    {
        const std::string& paramName = std::get<0>(param);
        bool isFloat = std::get<1>(param);
        if (isFloat)
        {
            *(float*)(&slot.params[i++]) = parser.get<float>(paramName);
        }
        else
        {
            slot.params[i++] = parser.get<uint32_t>(paramName);
        }
    }

    slots.push_back(slot);
    instances[slot.filterName] = std::move(instance);
    return true;
}

bool PipelineLayout::loadFplFromSource(const std::filesystem::path& path) noexcept
{
    std::cout << "Compiling pipeline " << path << " from source...\n";
    std::ifstream f(path);
    if (!f)
    {
        std::cout << "Error opening file " << path << "\n";
        return false;
    }

    std::vector<TexturePlaceholder> texPlaceholders{};
    texPlaceholders.push_back(TexturePlaceholder{.name = "input", .createdAt = -1, .tex = 0});

    int line{};
    std::string text;
    while (std::getline(f, text))
    {
        LineInfo info{};
        {
            LineParser lp(path, text, ++line);
            if (!lp.parse(info))
            {
                return false;
            }
        }
        if (info.empty)
        {
            continue;
        }

        FilterSlot filterSlot{};
        {
            for (const auto& in : info.inputs)
            {
                std::string_view inputName = std::get<std::string_view>(in.val);
                if (inputName == "output")
                {
                    printError(path, info.number, in.c, "Invalid syntax, reserved keyword 'output' can't be an input");
                    return false;
                }
                int texIndex = findTexPlaceholder(inputName, texPlaceholders);
                if (texIndex < 0)
                {
                    printError(path,
                               line,
                               in.c,
                               "Invalid syntax, '" +
                                   std::string(inputName) +
                                   "' requested as input, but has not been created yet");
                    return false;
                }
                filterSlot.inputs.push_back(texIndex);
            }
        }

        {
            filterSlot.filterName = std::string(std::get<std::string_view>(info.filterType.val));
            const auto& it = instances.find(filterSlot.filterName);
            if (it == instances.end())
            {
                instances[filterSlot.filterName] = std::make_unique<FilterInstance>(filterSlot.filterName);
                if (!instances[filterSlot.filterName]->valid())
                {
                    cleanup();
                    return false;
                }
            }
        }

        const auto& instance = instances[filterSlot.filterName];
        {
            if (instance->params.size() != info.params.size())
            {
                printError(path,
                           line,
                           info.filterType.c,
                           "Invalid syntax, expected " +
                               std::to_string(instance->params.size()) +
                               " parameter(s) in call to filter '" +
                               filterSlot.filterName +
                               "', found " +
                               std::to_string(info.params.size()) +
                               " instead\n");
                return false;
            }

            int i = 0;
            filterSlot.params.resize(instance->params.size());
            for (const auto& param : instance->params)
            {
                const std::string& paramName = std::get<0>(param);
                bool isFloat = std::get<1>(param);
                const auto& it = info.params.find(paramName);
                if (it == info.params.end())
                {
                    printError(path,
                               line,
                               info.filterType.c,
                               "Invalid syntax, parameter '" +
                                   paramName +
                                   "' not present in call to filter '" +
                                   filterSlot.filterName);
                    return false;
                }

                if (!isFloat && it->second.type == TokenType::INT)
                {
                    filterSlot.params[i++] = std::get<uint32_t>(it->second.val);
                }
                else if (isFloat && it->second.type == TokenType::FLOAT)
                {
                    *(float*)(&filterSlot.params[i++]) = std::get<float>(it->second.val);
                }
                else
                {
                    printError(path,
                               line,
                               info.filterType.c,
                               "Invalid syntax, parameter '" +
                                   paramName +
                                   "' in call to filter '" +
                                   filterSlot.filterName +
                                   "' is expected to be of type " +
                                   (isFloat ? "float" : "int"));
                    return false;
                }
            }
        }

        std::string_view outputName = std::get<std::string_view>(info.output.val);
        if (outputName == "input")
        {
            printError(path, info.number, info.output.c, "Invalid syntax, reserved keyword 'input' can't be an output");
            return false;
        }
        int texIndex = findTexPlaceholder(outputName, texPlaceholders);
        if (texIndex >= 0)
        {
            printError(
                path,
                line,
                info.output.c,
                "Invalid syntax, cannot overwrite '" + std::string(outputName) + "', must output to a new texture");
            return false;
        }

        std::string outputString = std::string(outputName);
        int iterations = info.iterations.type == TokenType::COUNT ? 1 : std::get<uint32_t>(info.iterations.val);
        if (iterations < 1)
        {
            printError(path,
                       line,
                       info.iterations.c,
                       "Invalid argument, iterations must be a positive non-null integer value");
            return false;
        }
        if (iterations > 1)
        {
            if (instance->inputCount > 1)
            {
                printError(
                    path, line, info.iterations.c, "Invalid syntax, filters with more than 1 input cannot be iterated");
                return false;
            }
            for (int i = 1; i < iterations; ++i)
            {
                filterSlot.outputTexture = texPlaceholders.size();
                texPlaceholders.push_back(TexturePlaceholder{.name = outputString + std::to_string(i)});
                slots.push_back(filterSlot);
                filterSlot.inputs[0] = filterSlot.outputTexture;
            }
        }
        filterSlot.outputTexture = texPlaceholders.size();
        texPlaceholders.push_back(TexturePlaceholder{.name = outputString});
        slots.push_back(filterSlot);
    }

    f.close();

    // find heights and sort by them
    int sweep = 0;
    while (sweep < slots.size())
    {
        for (int i = sweep; i < slots.size(); ++i)
        {
            int height = -1;
            for (const int& input : slots[i].inputs)
            {
                height = std::max(height, texPlaceholders[input].createdAt + 1);
            }
            if (height < VERY_BIG)
            {
                std::swap(slots[sweep], slots[i]);
                slots[sweep].height = height;
                texPlaceholders[slots[sweep].outputTexture].createdAt = height;
                for (const auto& input : slots[sweep].inputs)
                {
                    texPlaceholders[input].lastUsedAt = height;
                }
                ++sweep;
            }
        }
        if (sweep == 0)
        {
            std::cout << "Ill-formed fpl file, there must be at least one filter operation which only accepts the "
                         "keyword 'input' as input(s): "
                      << path << "\n";
            return false;
        }
    }

    // refer to filter slot not tex placeholder
    for (int i = 0; i < slots.size(); ++i)
    {
        texPlaceholders[slots[i].outputTexture].createdAt = i;
        for (auto& input : slots[i].inputs)
        {
            input = input == 0 ? -1 : texPlaceholders[input].createdAt;
        }
    }

    // calculate minimum number of textures actually needed and their indices in the filter slots
    std::vector<int> finalTexIndices{0};
    for (auto& filterSlot : slots)
    {
        bool found = false;
        for (int i = 0; i < finalTexIndices.size(); ++i)
        {
            int index = finalTexIndices[i];
            if (texPlaceholders[index].lastUsedAt < filterSlot.height)
            {
                texPlaceholders[index].lastUsedAt = filterSlot.height;
                finalTexIndices[i] = filterSlot.outputTexture;
                texPlaceholders[filterSlot.outputTexture].tex = texPlaceholders[index].tex;
                filterSlot.outputTexture = texPlaceholders[index].tex;
                found = true;
                break;
            }
        }
        if (!found)
        {
            int index = finalTexIndices.size();
            texPlaceholders[filterSlot.outputTexture].tex = index;
            finalTexIndices.push_back(filterSlot.outputTexture);
            filterSlot.outputTexture = index;
        }
    }
    texCount = finalTexIndices.size();
    std::cout << "Compilation finished successfully\n";
    serializeToCache(path.filename());
    return true;
}

bool PipelineLayout::serializeToCache(const std::string& pipelineName) const noexcept
{
    std::cout << "Serializing compiled pipeline and caching...\n";
    std::vector<uint32_t> toWrite{};
    toWrite.push_back(texCount);
    toWrite.push_back(slots.size());
    for (const auto& slot : slots)
    {
        toWrite.push_back(static_cast<uint32_t>(slot.height));
        toWrite.push_back(static_cast<uint32_t>(slot.outputTexture));
        utils::combine(slot.filterName, toWrite);
        toWrite.push_back(slot.inputs.size());
        for (const auto& input : slot.inputs)
        {
            toWrite.push_back(static_cast<uint32_t>(input));
        }
        toWrite.push_back(slot.params.size());
        for (const auto& param : slot.params)
        {
            toWrite.push_back(static_cast<uint32_t>(param));
        }
    }

    std::filesystem::path path{cachePath + pipelineName + ".cache"};
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

bool PipelineLayout::deserializeFromCache(const std::filesystem::path& path) noexcept
{
    const std::string& pipelineName = path.filename();
    std::filesystem::path fullCachePath{cachePath + pipelineName + ".cache"};
    if (!std::filesystem::exists(fullCachePath))
    {
        std::cout << "Cache file for " << pipelineName << " not found\n";
        return false;
    }
    if (std::filesystem::last_write_time(path) > std::filesystem::last_write_time(fullCachePath))
    {
        std::cout << "Cache file is too old, " << pipelineName << " needs to be recompiled\n";
        return false;
    }

    std::cout << "Found cache file for " << pipelineName << ", deserializing and loading pipeline...\n";

    std::ifstream f{fullCachePath, std::ios_base::binary};
    texCount = utils::readInt(f);
    slots.resize(utils::readInt(f));
    for (auto& slot : slots)
    {
        slot.height = utils::readInt(f);
        slot.outputTexture = utils::readInt(f);
        slot.filterName = utils::uncombine(f);
        const auto& it = instances.find(slot.filterName);
        if (it == instances.end())
        {
            instances[slot.filterName] = std::make_unique<FilterInstance>(slot.filterName);
            if (!instances[slot.filterName]->valid())
            {
                cleanup();
                return false;
            }
        }

        slot.inputs.resize(utils::readInt(f));
        for (auto& input : slot.inputs)
        {
            input = utils::readInt(f);
        }
        slot.params.resize(utils::readInt(f));
        for (auto& param : slot.params)
        {
            param = utils::readInt(f);
        }
    }
    f.close();

    std::cout << "Deserializing finished successfully\n";
    return true;
}
} // namespace flint::fpl
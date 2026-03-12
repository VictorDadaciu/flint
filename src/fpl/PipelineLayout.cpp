#include "fpl/PipelineLayout.h"

#include "FilterUtils.h"
#include "cmdparser.hpp"
#include "fpl/LineParser.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <single_include/nlohmann/json.hpp>
#include <string>
#include <variant>

namespace flint::fpl
{
static constexpr int VERY_BIG = 1000000;
static constexpr const char* cachePath = "/home/victordadaciu/workspace/flint/pipelines/.cache/";

// TODO maybe store these a little differently, unordered_map maybe?
struct TexturePlaceholder final
{
    std::string name{};
    int createdAt = VERY_BIG; // first height, then filter slot
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
        if (!createFromFilterName(filter))
        {
            m_valid = false;
            return;
        }
    }
    else
    {
        std::filesystem::create_directory(cachePath);
        std::filesystem::path path{filter};
        if (!deserializeFromCache(path.filename()) && !loadFplFromSource(path))
        {
            m_valid = false;
            return;
        }
    }
    m_valid = true;
}

bool PipelineLayout::createFromFilterName(const std::string& filter) noexcept
{
    texCount = 2;

    FilterSlot slot{};
    slot.inputs = {-1};
    slot.outputTexture = 1;
    const auto& it = toFilterType.find(filter);
    if (it == toFilterType.end())
    {
        std::cout << "Invalid filter type requested\n";
        return false;
    }
    slot.type = it->second;
    slots.push_back(slot);
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
            std::string filterName = std::string(std::get<std::string_view>(info.filterType.val));
            const auto& it = toFilterType.find(filterName);
            if (it == toFilterType.end())
            {
                printError(path, line, info.filterType.c, "Invalid syntax, invalid filter name");
                return false;
            }
            filterSlot.type = it->second;

            auto paramsMap = utils::parameterMap(filterSlot.type);
            for (const auto& token : info.params)
            {
                if (token.second.type == TokenType::FLOAT)
                {
                    paramsMap[std::string(token.first)] = std::get<float>(token.second.val);
                }
                else
                {
                    paramsMap[std::string(token.first)] = std::get<uint32_t>(token.second.val);
                }
            }
            if (!utils::validateMap(filterSlot.type, paramsMap))
            {
                return false;
            }
            filterSlot.params.vals.resize(paramsMap.size());
            filterSlot.params.isFloat.resize(paramsMap.size());
            int i = 0;
            for (const auto& param : paramsMap)
            {
                if (std::holds_alternative<uint32_t>(param.second))
                {
                    filterSlot.params.isFloat[i] = false;
                    filterSlot.params.vals[i] = std::get<uint32_t>(param.second);
                }
                else
                {
                    filterSlot.params.isFloat[i] = true;
                    *(float*)(&filterSlot.params.vals[i]) = std::get<float>(param.second);
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
        // TODO restrict iterations to filters that take only 1 input
        if (info.iterations > 1)
        {
            for (int i = 1; i < info.iterations; ++i)
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

static nlohmann::json slotToJson(const FilterSlot& slot) noexcept
{
    nlohmann::json json{
        {"type", (int)slot.type},
        {"height", slot.height},
        {"outputTexture", slot.outputTexture},
        {"inputs", nlohmann::json::array()},
        {"params", nlohmann::json::array()},
    };

    auto& inputsJson = json["inputs"];
    for (int input : slot.inputs)
    {
        inputsJson.push_back(input);
    }

    auto& paramsJson = json["params"];
    for (int i = 0; i < slot.params.vals.size(); ++i)
    {
        if (slot.params.isFloat[i])
        {
            paramsJson.push_back(nlohmann::json{{"isFloat", true}, {"value", *(float*)(&slot.params.vals[i])}});
        }
        else
        {
            paramsJson.push_back(nlohmann::json{{"isFloat", false}, {"value", slot.params.vals[i]}});
        }
    }
    return json;
}

bool PipelineLayout::serializeToCache(const std::string& pipelineName) const noexcept
{
    std::cout << "Serializing compiled pipeline and caching...\n";
    nlohmann::json json{{"texCount", texCount}, {"slots", nlohmann::json::array()}};
    auto& slotsJson = json["slots"];
    for (const auto& slot : slots)
    {
        slotsJson.push_back(slotToJson(slot));
    }

    std::filesystem::path path{cachePath + pipelineName + ".json"};
    std::ofstream f{path};
    if (!f)
    {
        std::cout << "Error opening cache file for write " << path << "\n";
        return false;
    }
    f << std::setw(4) << json << std::endl;
    f.close();
    std::cout << "Caching finished successfully\n";
    return true;
}

static void jsonToSlot(const nlohmann::json& json, FilterSlot& slot)
{
    slot.height = json["height"].get<int>();
    slot.outputTexture = json["outputTexture"].get<int>();
    slot.type = (FilterType)json["type"].get<int>();

    auto& inputsJson = json["inputs"];
    slot.inputs.resize(inputsJson.size());
    for (int i = 0; i < slot.inputs.size(); ++i)
    {
        slot.inputs[i] = inputsJson[i].get<int>();
    }

    auto& paramsJson = json["params"];
    slot.params.vals.resize(paramsJson.size());
    slot.params.isFloat.resize(paramsJson.size());
    for (int i = 0; i < slot.params.vals.size(); ++i)
    {
        slot.params.isFloat[i] = paramsJson[i]["isFloat"].get<bool>();
        if (slot.params.isFloat[i])
        {
            *(float*)(&slot.params.vals[i]) = paramsJson[i]["value"].get<float>();
        }
        else
        {
            slot.params.vals[i] = paramsJson[i]["value"].get<uint32_t>();
        }
    }
}

bool PipelineLayout::deserializeFromCache(const std::string& pipelineName) noexcept
{
    try
    {
        std::filesystem::path path{cachePath + pipelineName + ".json"};
        if (!std::filesystem::exists(path))
        {
            std::cout << "Cache file for " << pipelineName << " not found\n";
            return false;
        }

        std::ifstream f{path};
        if (!f)
        {
            std::cout << "Error opening cache file for read " << path << "\n";
            return false;
        }
        std::cout << "Found cache file for " << pipelineName << ", deserializing and loading pipeline...\n";
        nlohmann::json json{};
        f >> json;
        f.close();

        texCount = json["texCount"].get<int>();

        auto& slotsJson = json["slots"];
        slots.resize(json["slots"].size());
        for (int i = 0; i < slots.size(); ++i)
        {
            jsonToSlot(slotsJson[i], slots[i]);
        }
        std::cout << "Deserializing finished successfully\n";
        return true;
    }
    catch (...)
    {
        std::cout << "Ill-formed cache file, recompiling " << pipelineName << "\n";
        return false;
    }
}
} // namespace flint::fpl
#include "fpl/PipelineLayout.h"

#include "Error.h"
#include "FilterInstance.h"
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

PipelineLayout::PipelineLayout(const Args& args) noexcept
{
    std::cout << "Compiling pipeline '" << args.filterPath.filename().string() << "'...\n";
    std::ifstream f(args.filterPath);
    if (!f)
    {
        fail("Error opening fpl file '" + args.filterPath.string() + "'");
    }

    std::vector<TexturePlaceholder> texPlaceholders{};
    texPlaceholders.push_back(TexturePlaceholder{.name = "input", .createdAt = -1, .tex = 0});

    int line{};
    std::string text;
    while (std::getline(f, text))
    {
        LineInfo info = LineParser(args.filterPath, text, ++line).parse();
        if (info.empty)
        {
            continue;
        }

        FilterSlot filterSlot{};
        {
            filterSlot.filterName = std::string(std::get<std::string_view>(info.filterType.val));
            const auto& it = instances.find(filterSlot.filterName);
            if (it == instances.end())
            {
                instances[filterSlot.filterName] = std::make_unique<FilterInstance>(filterSlot.filterName);
            }
        }

        const auto& instance = instances[filterSlot.filterName];
        {
            if (instance->params.size() != info.params.size())
            {
                fail(fplErrorPrefix(args.filterPath, line, info.filterType.c) +
                     "Invalid syntax, expected " +
                     std::to_string(instance->params.size()) +
                     " parameter(s) in call to filter '" +
                     filterSlot.filterName +
                     "', found " +
                     std::to_string(info.params.size()) +
                     " instead\n");
            }

            int i = 0;
            filterSlot.params.resize(instance->params.size());
            for (const auto& param : instance->params)
            {
                const std::string& paramName = std::get<0>(param);
                bool isFloat = std::get<1>(param);

                const auto& it = info.params.find(info.namedParams ? paramName : std::to_string(i));
                if (it == info.params.end())
                {
                    fail(fplErrorPrefix(args.filterPath, line, info.filterType.c) +
                         "Invalid syntax, parameter '" +
                         paramName +
                         "' not present in call to filter '" +
                         filterSlot.filterName);
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
                    fail(fplErrorPrefix(args.filterPath, line, info.filterType.c) +
                         "Invalid syntax, parameter '" +
                         paramName +
                         "' in call to filter '" +
                         filterSlot.filterName +
                         "' is expected to be of type " +
                         (isFloat ? "float" : "int"));
                }
            }
        }

        {
            if (info.inputs.size() != instance->inputCount)
            {
                fail(fplErrorPrefix(args.filterPath, line, info.inputs[0].c) +
                     "Invalid syntax, filter '" +
                     filterSlot.filterName +
                     "' expects " +
                     std::to_string(instance->inputCount) +
                     " inputs, but got " +
                     std::to_string(info.inputs.size()));
            }

            for (const auto& in : info.inputs)
            {
                std::string_view inputName = std::get<std::string_view>(in.val);
                if (inputName == "output")
                {
                    fail(fplErrorPrefix(args.filterPath, line, in.c) +
                         "Invalid syntax, reserved keyword 'output' can't be an input");
                }
                int texIndex = findTexPlaceholder(inputName, texPlaceholders);
                if (texIndex < 0)
                {
                    fail(fplErrorPrefix(args.filterPath, line, in.c) +
                         "Invalid syntax, '" +
                         std::string(inputName) +
                         "' requested as input, but has not been created yet");
                }
                filterSlot.inputs.push_back(texIndex);
            }
        }

        std::string_view outputName = std::get<std::string_view>(info.output.val);
        if (outputName == "input")
        {
            fail(fplErrorPrefix(args.filterPath, line, info.output.c) +
                 "Invalid syntax, reserved keyword 'input' can't be an output");
        }
        int texIndex = findTexPlaceholder(outputName, texPlaceholders);
        if (texIndex >= 0)
        {
            fail(fplErrorPrefix(args.filterPath, line, info.output.c) +
                 "Invalid syntax, cannot overwrite '" +
                 std::string(outputName) +
                 "', must output to a new texture");
        }

        std::string outputString = std::string(outputName);
        int iterations = info.iterations.type == TokenType::COUNT ? 1 : std::get<uint32_t>(info.iterations.val);
        if (iterations < 1)
        {
            fail(fplErrorPrefix(args.filterPath, line, info.iterations.c) +
                 "Invalid argument, iterations must be a positive non-null integer value");
        }
        if (iterations > 1)
        {
            if (instance->inputCount > 1)
            {
                fail(fplErrorPrefix(args.filterPath, line, info.iterations.c) +
                     "Invalid syntax, filters with more than 1 input cannot be iterated");
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
            fail("Ill-formed fpl file '" +
                 args.filterPath.string() +
                 "', there must be at least one filter operation which only accepts the keyword 'input' as input(s)");
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
}

PipelineLayout::PipelineLayout(PipelineLayout&& other) noexcept :
    instances(std::move(other.instances)), slots(std::move(other.slots)), texCount(other.texCount)
{
    other.texCount = 0;
}

void PipelineLayout::operator=(PipelineLayout&& other) noexcept
{
    instances = std::move(other.instances);
    slots = std::move(other.slots);
    texCount = other.texCount;
    other.texCount = 0;
}
} // namespace flint::fpl
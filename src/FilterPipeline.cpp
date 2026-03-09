#include "FilterPipeline.h"

#include "FilterInstance.h"
#include "FilterUtils.h"
#include "StagingBuffer.h"
#include "SubmissionInfo.h"
#include "SubmissionStack.h"
#include "Texture.h"
#include "VkContext.h"
#include "cmdparser.hpp"

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace flint
{
enum class TokenType
{
    INPUT,
    OUTPUT,
    OPEN_PAR,
    CLOSED_PAR,
    ARROW,
    COMMA,
    STRING,
    INT,
    FLOAT,
    COUNT
};

struct TokenConstructor final
{
    TokenType type = TokenType::COUNT;

    std::string val{};

    int c = -1;
};

struct Token final

{
    TokenType type = TokenType::COUNT;

    std::variant<std::string, int, float> val{};

    int c = -1;
};

constexpr const char* simpleTokens = "(),>";

static bool isSimpleToken(char c) noexcept
{
    for (int i = 0; i < strlen(simpleTokens); ++i)
    {
        if (c == simpleTokens[i])
        {
            return true;
        }
    }
    return false;
}

static void printError(const std::string& path, int line, int col, const std::string& msg) noexcept
{
    std::cout << "fpl ERROR " << path << " (" << line << ", " << col << "): " << msg << "\n";
}

static bool endComplexToken(const std::string& path, int lineNumber, const std::string& value, Token& token) noexcept
{
    switch (token.type)
    {
    case TokenType::STRING:
        token.val = value;
        if (value == "input")
        {
            token.type = TokenType::INPUT;
        }
        else if (value == "output")
        {
            token.type = TokenType::OUTPUT;
        }
        break;
    case TokenType::INT:
        try
        {
            token.val = std::stoi(value.c_str());
        }
        catch (const std::invalid_argument& e)
        {
            printError(path, lineNumber, token.c, "Invalid syntax, could not convert token to integer " + value);
            return false;
        }
        break;
    case TokenType::FLOAT:
        try
        {
            token.val = std::stof(value.c_str());
        }
        catch (const std::invalid_argument& e)
        {
            printError(path, lineNumber, token.c, "Invalid syntax, could not convert token to float " + value);
            return false;
        }
        break;
    default:
        printError(path, lineNumber, token.c, "Invalid token parsed, this shouldn't be happening " + value);
        return false;
    }
    return true;
}

static bool
lexLine(const std::string& path, const std::string& line, int lineNumber, std::vector<Token>& tokens) noexcept
{
    TokenConstructor currentToken{};
    for (int col = 0; col <= line.size(); ++col)
    {
        int fileCol = col + 1;
        bool end = col == line.size();
        char c = end ? '\0' : line[col];
        bool letter = std::isalpha(c);
        bool digit = std::isdigit(c);
        bool dot = c == '.';
        bool under = c == '_';
        bool dash = c == '-';
        bool simple = isSimpleToken(c);
        bool space = std::isspace(c);
        if (!(letter || digit || dot || under || dash || simple || space || end))
        {
            printError(path, lineNumber, fileCol, "Invalid character " + std::string(1, c));
            return false;
        }

        if (simple || end || space)
        {
            // complex token ends here if exists
            if (!currentToken.val.empty())
            {
                Token token{.type = currentToken.type, .c = currentToken.c};
                if (!endComplexToken(path, lineNumber, currentToken.val, token))
                {
                    return false;
                }
                tokens.push_back(token);
                currentToken = TokenConstructor{};
            }
            if (simple)
            {
                Token token{.val = std::string(1, c), .c = fileCol};
                switch (c)
                {
                case '>':
                    token.type = TokenType::ARROW;
                    break;
                case ',':
                    token.type = TokenType::COMMA;
                    break;
                case '(':
                    token.type = TokenType::OPEN_PAR;
                    break;
                case ')':
                    token.type = TokenType::CLOSED_PAR;
                    break;
                default:
                    printError(path,
                               lineNumber,
                               fileCol,
                               "Invalid character parsed, this shouldn't be happening " + std::string(1, c));
                    return false;
                }
                tokens.push_back(token);
            }
            continue;
        }

        // complex token starts
        if (currentToken.val.empty())
        {
            currentToken.c = fileCol;
            currentToken.val = std::string(1, c);
            if (letter || under)
            {
                currentToken.type = TokenType::STRING;
            }
            else if (digit || dash)
            {
                currentToken.type = TokenType::INT;
            }
            else if (dot)
            {
                currentToken.type = TokenType::FLOAT;
            }
            continue;
        }
        // complex token continues
        else
        {
            currentToken.val += c;
            if (dash)
            {
                printError(path,
                           lineNumber,
                           fileCol,
                           "Invalid syntax, dashes (-) can only be found at the beginning of a number");
                return false;
            }
            else if (under && currentToken.type != TokenType::STRING)
            {
                printError(path, lineNumber, fileCol, "Invalid syntax, numbers cannot contain underscores (_)");
                return false;
            }
            else if (letter && currentToken.type != TokenType::STRING)
            {
                printError(
                    path, lineNumber, currentToken.c, "Invalid syntax, strings must begin with a letter (a-zA-Z)");
                return false;
            }
            else if (dot)
            {
                switch (currentToken.type)
                {
                case TokenType::INT:
                    currentToken.type = TokenType::FLOAT;
                    break;
                case TokenType::FLOAT:
                    printError(
                        path, lineNumber, fileCol, "Invalid syntax, numbers cannot contain more than one dot (.)");
                    return false;
                default:
                }
            }
            continue;
        }

        printError(path, lineNumber, fileCol, "Invalid character, this shouldn't be happening " + std::string(1, c));
        return false;
    }
    return true;
}

#ifndef NDEBUG
static std::string tokenTypeToStr(TokenType type) noexcept
{
    switch (type)
    {
    case TokenType::ARROW:
        return "ARROW";
    case TokenType::OPEN_PAR:
        return "OPEN_PAR";
    case TokenType::CLOSED_PAR:
        return "CLOSED_PAR";
    case TokenType::COMMA:
        return "COMMA";
    case TokenType::INPUT:
        return "INPUT";
    case TokenType::OUTPUT:
        return "OUTPUT";
    case TokenType::INT:
        return "INT";
    case TokenType::FLOAT:
        return "FLOAT";
    case TokenType::STRING:
        return "STRING";
    case TokenType::COUNT:
        return "COUNT";
    }
}

static void printToken(int lineNumber, const Token& token) noexcept
{
    std::cout << "{ " << "(" << lineNumber << ", " << token.c << "), " << tokenTypeToStr(token.type) << ", ";
    std::visit(
        [](const auto& value)
        {
            std::cout << "'" << value << "' } \n";
        },
        token.val);
}
#endif

std::vector<std::vector<Token>> lex(const std::string& path) noexcept
{
    std::vector<std::vector<Token>> ret{};
    std::ifstream f(path);
    if (!f)
    {
        std::cout << "Parsing error: Could not open file " + path + "\n";
        return {};
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(f, line))
    {
        if (line.empty())
        {
            ++lineNumber;
            continue;
        }
        std::vector<Token> tokens{};
        if (!lexLine(path, line, ++lineNumber, tokens))
        {
            return {};
        }
        ret.push_back(tokens);
#ifndef NDEBUG
        for (const Token& token : tokens)
        {
            printToken(lineNumber, token);
        }
        std::cout << "\n";
#endif
    }
    return ret;
}

bool FilterPipeline::createSimplePipeline(const std::string& filter) noexcept
{
    m_uniqueTexturesCount = 2;

    FilterSlot slot{};
    slot.inputFilterSlots = {-1};
    slot.outputTexture = 1;
    const auto& it = toFilterType.find(filter);
    if (it == toFilterType.end())
    {
        std::cout << "Invalid filter type requested\n";
        return false;
    }
    slot.type = it->second;
    m_filterSlots.push_back(slot);

    m_filterInstances[slot.type] =
        std::make_unique<FilterInstance>("/home/victordadaciu/workspace/flint/compute/" + filter + ".comp.spv");
    return true;
}

constexpr int VERY_BIG = 1000000;

struct TexturePlaceholder final
{
    std::string name{};
    int createdAt = VERY_BIG;
    int lastUsedAt = -1;
    int tex = -1;
};

static int findTexPlaceholder(const std::string& name, const std::vector<TexturePlaceholder>& texPlaceholders) noexcept
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

template<class>
inline constexpr bool always_false_v = false;

static std::string variantToString(const std::variant<std::string, int, float>& variant) noexcept
{
    return std::visit(
        [](const auto& var)
        {
            using T = std::decay_t<decltype(var)>;
            if constexpr (std::is_same_v<T, std::string>)
            {
                return var;
            }
            else if constexpr (std::is_arithmetic_v<T>)
            {
                return std::to_string(var);
            }
            else
            {
                static_assert(always_false_v<T>, "Cannot convert type to string");
            }
        },
        variant);
}

bool FilterPipeline::createComplexPipeline(const std::string& path) noexcept
{
    const auto& commands = lex(path);
    if (commands.empty())
    {
        return false;
    }

    std::vector<TexturePlaceholder> texPlaceholders{};
    texPlaceholders.push_back(TexturePlaceholder{.name = "input", .createdAt = -1, .tex = 0});

    // TODO add lines to tokens
    int line = 0;
    for (const auto& cmd : commands)
    {
        ++line;
        // TODO this is really ugly, wanna do it some other way
        bool processedInput{};
        bool processedFilterName{};
        bool processedFilterParams{};
        bool processedCommand{};
        bool processedOutput{};

        FilterSlot filterSlot{.height = VERY_BIG};
        for (const auto& token : cmd)
        {
            if (!processedInput)
            {
                if (token.type == TokenType::INPUT)
                {
                    filterSlot.inputFilterSlots.push_back(0);
                    continue;
                }
                else if (token.type == TokenType::STRING)
                {
                    const std::string& texName = std::get<std::string>(token.val);
                    int texIndex = findTexPlaceholder(texName, texPlaceholders);
                    if (texIndex < 0)
                    {
                        printError(
                            path,
                            line,
                            token.c,
                            "Invalid syntax, '" + texName + "' requested as input, but has not been created yet");
                        return false;
                    }
                    filterSlot.inputFilterSlots.push_back(texIndex);
                    continue;
                }
                else if (token.type == TokenType::COMMA)
                {
                    continue;
                }
                else if (token.type == TokenType::ARROW)
                {
                    processedInput = true;
                    continue;
                }
                else if (token.type == TokenType::OUTPUT)
                {
                    printError(path,
                               line,
                               token.c,
                               "Invalid syntax, reserved keyword 'output' cannot be used as input to filter");
                    return false;
                }
                printError(path,
                           line,
                           token.c,
                           "Invalid syntax, unexpected token in input section '" + variantToString(token.val) + "'");
                return false;
            }
            else if (!processedFilterName)
            {
                std::string val = variantToString(token.val);
                if (token.type == TokenType::STRING && filterSlot.type == FilterType::count)
                {
                    const auto& it = toFilterType.find(val);
                    if (it == toFilterType.end())
                    {
                        printError(path, line, token.c, "Invalid syntax, invalid filter name");
                        return false;
                    }
                    filterSlot.type = it->second;
                    if (m_filterInstances.find(filterSlot.type) == m_filterInstances.end())
                    {
                        m_filterInstances[filterSlot.type] = std::make_unique<FilterInstance>(
                            "/home/victordadaciu/workspace/flint/compute/" + val + ".comp.spv");
                    }
                    continue;
                }
                else if (token.type == TokenType::OPEN_PAR && filterSlot.type != FilterType::count)
                {
                    processedFilterName = true;
                    continue;
                }
                printError(path, line, token.c, "Invalid syntax, expected filter name, found '" + val + "'");
                return false;
            }
            else if (!processedFilterParams)
            {
                // TODO actually implement this
                if (token.type == TokenType::CLOSED_PAR)
                {
                    processedFilterParams = true;
                    continue;
                }
                printError(
                    path, line, token.c, "Invalid syntax, expected ')', found '" + variantToString(token.val) + "'");
                return false;
            }
            else if (!processedCommand)
            {
                if (token.type == TokenType::ARROW)
                {
                    processedCommand = true;
                    continue;
                }
                printError(
                    path, line, token.c, "Invalid syntax, expected '>', found '" + variantToString(token.val) + "'");
                return false;
            }
            else if (!processedOutput)
            {
                if (filterSlot.outputTexture != -1)
                {
                    printError(
                        path,
                        line,
                        token.c,
                        "Invalid syntax, output section must contain the name of only one unique output texture");
                    return false;
                }
                if (token.type == TokenType::STRING || token.type == TokenType::OUTPUT)
                {
                    const std::string& texName = std::get<std::string>(token.val);
                    int texIndex = findTexPlaceholder(texName, texPlaceholders);
                    if (texIndex != -1)
                    {
                        printError(path, line, token.c, "Invalid syntax, cannot overwrite previously created texture");
                        return false;
                    }
                    filterSlot.outputTexture = texPlaceholders.size();
                    texPlaceholders.push_back(TexturePlaceholder{.name = texName});
                    continue;
                }
                else if (token.type == TokenType::INPUT)
                {
                    printError(path, line, token.c, "Invalid syntax, cannot overwrite the input texture");
                    return false;
                }
                printError(
                    path,
                    line,
                    token.c,
                    "Invalid syntax, expected a new unique texture name, found '" + variantToString(token.val) + "'");
                return false;
            }
        }
        processedOutput = true;

        if (!(processedInput && processedFilterName && processedFilterParams && processedCommand && processedOutput))
        {
            printError(path, line, 0, "Ill-formed line, unexpected error, this shouldn't happen");
            return false;
        }
        m_filterSlots.push_back(filterSlot);
    }

    // find heights and sort by them
    int sweep = 0;
    while (sweep < m_filterSlots.size())
    {
        for (int i = sweep; i < m_filterSlots.size(); ++i)
        {
            int height = -1;
            for (const int& input : m_filterSlots[i].inputFilterSlots)
            {
                height = std::max(height, texPlaceholders[input].createdAt + 1);
            }
            if (height < VERY_BIG)
            {
                std::swap(m_filterSlots[sweep], m_filterSlots[i]);
                m_filterSlots[sweep].height = height;
                texPlaceholders[m_filterSlots[sweep].outputTexture].createdAt = height;
                for (const auto& input : m_filterSlots[sweep].inputFilterSlots)
                {
                    texPlaceholders[input].lastUsedAt = height;
                }
                ++sweep;
            }
        }
        if (sweep == 0)
        {
            printError(path,
                       0,
                       0,
                       "Ill-formed fpl file, there must be at least one filter operation which only has the keyword "
                       "'input' as inputs");
            return false;
        }
    }

    // refer to filter slot not tex placeholder
    for (int i = 0; i < m_filterSlots.size(); ++i)
    {
        for (auto& input : m_filterSlots[i].inputFilterSlots)
        {
            input = input == 0 ? -1 : texPlaceholders[input].createdAt;
        }
        texPlaceholders[m_filterSlots[i].outputTexture].createdAt = i;
    }

    // calculate minimum number of textures actually needed and their indices in the filter slots
    std::vector<int> finalTexIndices{0};
    for (auto& filterSlot : m_filterSlots)
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
    m_uniqueTexturesCount = finalTexIndices.size();
    return true;
}

FilterPipeline::FilterPipeline(const cli::Parser& args) noexcept
{
    std::string filter = args.get<std::string>("f");
    if (filter.ends_with(".fpl"))
    {
        if (!createComplexPipeline(filter))
        {
            m_valid = false;
            return;
        }
    }
    else
    {
        if (!createSimplePipeline(filter))
        {
            m_valid = false;
            return;
        }
    }
    for (const auto& instance : m_filterInstances)
    {
        if (!instance.second->valid())
        {
            m_valid = false;
            return;
        }
    }
    m_valid = true;
}

void FilterPipeline::cleanup() noexcept
{
    for (auto& instance : m_filterInstances)
    {
        instance.second->cleanup();
    }
}

static bool transitionToTransfer(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    if (!submissions[transfer].begin(VK_PIPELINE_STAGE_TRANSFER_BIT))
    {
        return false;
    }

    auto barrier = createImageMemoryBarrier();
    barrier.image = tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(submissions[transfer].commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    tex.lastSubmissionIndex = transfer;
    return submissions[transfer].end();
}

static bool copyImage(Texture& tex, SubmissionStack& submissions) noexcept
{
    int copyImage = submissions.get();
    if (!submissions[copyImage].begin())
    {
        return false;
    }

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {(uint32_t)imageMetadata.width, (uint32_t)imageMetadata.height, 1};

    vkCmdCopyBufferToImage(submissions[copyImage].commandBuffer,
                           stagingBuffer.buffer,
                           tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);

    submissions[copyImage].prereqs.push_back(tex.lastSubmissionIndex);
    tex.lastSubmissionIndex = copyImage;
    return submissions[copyImage].end();
}

static bool transitionToComputeFromTransfer(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    if (!submissions[transfer].begin())
    {
        return false;
    }

    auto barrier = createImageMemoryBarrier();
    barrier.image = tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(submissions[transfer].commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    submissions[transfer].prereqs.push_back(tex.lastSubmissionIndex);
    tex.lastSubmissionIndex = transfer;
    return submissions[transfer].end();
}

static bool transitionToComputeFromUndefined(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    if (!submissions[transfer].begin())
    {
        return false;
    }

    auto barrier = createImageMemoryBarrier();
    barrier.image = tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(submissions[transfer].commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    tex.lastSubmissionIndex = transfer;
    return submissions[transfer].end();
}

bool FilterPipeline::applyFilter(std::vector<Texture>& texes,
                                 const FilterSlot& filterSlot,
                                 SubmissionStack& submissions) const noexcept
{
    int compute = submissions.get();
    FilterInstance* filter = m_filterInstances[filterSlot.type].get();
    if (!(submissions[compute].begin()))
    {
        return false;
    }

    vkCmdBindPipeline(submissions[compute].commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, filter->pipeline);

    for (int i = 0; i < filterSlot.inputFilterSlots.size(); ++i)
    {
        int index = filterSlot.inputFilterSlots[i];
        Texture& tex = index == -1 ? texes[0] : texes[m_filterSlots[index].outputTexture];
        vkCmdBindDescriptorSets(submissions[compute].commandBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                filter->pipelineLayout,
                                i,
                                1,
                                &tex.descriptorSet,
                                0,
                                nullptr);
        submissions[compute].prereqs.push_back(tex.lastSubmissionIndex);
        tex.lastSubmissionIndex = compute;
    }
    vkCmdBindDescriptorSets(submissions[compute].commandBuffer,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            filter->pipelineLayout,
                            filterSlot.inputFilterSlots.size(),
                            1,
                            &texes[filterSlot.outputTexture].descriptorSet,
                            0,
                            nullptr);
    submissions[compute].prereqs.push_back(texes[filterSlot.outputTexture].lastSubmissionIndex);
    texes[filterSlot.outputTexture].lastSubmissionIndex = compute;

    vkCmdDispatch(submissions[compute].commandBuffer, imageMetadata.groupX, imageMetadata.groupY, 1);

    return submissions[compute].end();
}

static bool transitionToTransferFromCompute(Texture& tex, SubmissionStack& submissions) noexcept
{
    int transfer = submissions.get();
    if (!submissions[transfer].begin(VK_PIPELINE_STAGE_TRANSFER_BIT))
    {
        return false;
    }

    auto barrier = createImageMemoryBarrier();
    barrier.image = tex.image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(
        submissions[transfer].commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    submissions[transfer].prereqs.push_back(tex.lastSubmissionIndex);
    tex.lastSubmissionIndex = transfer;
    return submissions[transfer].end();
}

static bool copyImageToBuffer(Texture& tex, SubmissionStack& submissions) noexcept
{
    int copyImage = submissions.get();
    if (!submissions[copyImage].begin())
    {
        return false;
    }

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {(uint32_t)imageMetadata.width, (uint32_t)imageMetadata.height, 1};

    vkCmdCopyImageToBuffer(submissions[copyImage].commandBuffer,
                           tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           stagingBuffer.buffer,
                           1,
                           &region);

    submissions[copyImage].prereqs.push_back(tex.lastSubmissionIndex);
    tex.lastSubmissionIndex = copyImage;
    return submissions[copyImage].end();
}

bool FilterPipeline::record(std::vector<Texture>& texes, SubmissionStack& submissions) const noexcept
{
    assert(texes.size() == m_uniqueTexturesCount);
    if (!(transitionToTransfer(texes[0], submissions) &&
          copyImage(texes[0], submissions) &&
          transitionToComputeFromTransfer(texes[0], submissions)))
    {
        return false;
    }

    for (int i = 1; i < texes.size(); ++i)
    {
        if (!transitionToComputeFromUndefined(texes[i], submissions))
        {
            return false;
        }
    }

    for (int i = 0; i < m_filterSlots.size(); ++i)
    {
        if (!applyFilter(texes, m_filterSlots[i], submissions))
        {
            return false;
        }
    }

    Texture& tex = texes[m_filterSlots[m_filterSlots.size() - 1].outputTexture];
    return transitionToTransferFromCompute(tex, submissions) && copyImageToBuffer(tex, submissions);
}
} // namespace flint
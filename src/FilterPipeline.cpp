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
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace flint
{
enum class TokenType
{
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

    std::variant<std::string_view, int, float> val{};

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

static void printError(const std::string_view& path, int line, int col, const std::string& msg) noexcept
{
    std::cout << "fpl ERROR " << path << " (" << line << ", " << col << "): " << msg << "\n";
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

struct LineInfo final
{
    std::vector<Token> inputs{};
    Token filterType{};
    Token output{};
    int iterations = 1;
    int number{};

    inline bool valid() const noexcept
    {
        return !inputs.empty() &&
               filterType.type ==
               TokenType::STRING &&
               output.type ==
               TokenType::STRING &&
               number > 0;
    }
};

class LineParser final
{
public:
    LineParser(std::string_view path, std::string_view line, int number) noexcept :
        m_path(path), m_text(line), m_number(number)
    {
    }

    bool parse(LineInfo& info) noexcept
    {
        info.number = m_number;
        if (!lex())
        {
            return false;
        }
        if (m_tokens.empty())
        {
            return true;
        }

#ifndef NDEBUG
        for (const auto& t : m_tokens)
        {
            printToken(info.number, t);
        }
        std::cout << "\n";
#endif

        m_index = 0;
        // input section
        if (!expect({TokenType::STRING}))
        {
            printError(m_path,
                       m_number,
                       m_tokens[m_index].c,
                       "Invalid syntax, line should start with least one input texture name");
            return false;
        }
        info.inputs.push_back(m_tokens[m_index]);
        advance();

        while (expect({TokenType::COMMA, TokenType::STRING}))
        {
            advance();
            info.inputs.push_back(m_tokens[m_index]);
            advance();
        }

        if (!expect({TokenType::ARROW}))
        {
            printError(m_path,
                       m_number,
                       m_tokens[m_index].c,
                       "Invalid syntax, arrow character (>) expected after input section");
            return false;
        }
        advance();

        // filter section
        if (!expect({TokenType::STRING, TokenType::OPEN_PAR, TokenType::CLOSED_PAR}))
        {
            printError(m_path, m_number, m_tokens[m_index].c, "Invalid syntax, filter call expected");
            return false;
        }
        info.filterType = m_tokens[m_index];
        advance(3);

        if (expect({TokenType::INT}))
        {
            info.iterations = std::get<int>(m_tokens[m_index].val);
            if (info.iterations < 1)
            {
                printError(m_path,
                           m_number,
                           m_tokens[m_index].c,
                           "Invalid argument, iterations must be a positive non-null integer value");
                return false;
            }
            advance();
        }

        if (!expect({TokenType::ARROW}))
        {
            printError(m_path, m_number, m_tokens[m_index].c, "Invalid syntax, arrow to output section expected");
            return false;
        }
        advance();

        // output section
        if (!expect({TokenType::STRING}))
        {
            printError(
                m_path, m_number, m_tokens[m_index].c, "Invalid syntax, expected at least one output texture name");
            return false;
        }
        info.output = m_tokens[m_index];
        advance();

        if (m_index < m_tokens.size())
        {
            printError(m_path,
                       m_number,
                       m_tokens[m_index].c,
                       "Invalid syntax, too many tokens in output section, expected only an output texture name");
            return false;
        }
        return true;
    }

private:
    inline void advance(int by = 1) noexcept { m_index += by; }

    bool lex() noexcept
    {
        while (m_index < m_text.size())
        {
            Token t{};
            if (!processToken(t))
            {
                return false;
            }
            else if (t.type == TokenType::COUNT)
            {
                return true;
            }
            m_tokens.push_back(t);
        }
        return true;
    }

    bool processToken(Token& t) noexcept
    {
        do
        {
            char c = m_text[m_index++];
            bool space = std::isspace(c);
            bool digit = std::isdigit(c);
            bool letter = std::isalpha(c);
            bool simple = isSimpleToken(c);
            bool hash = c == '#';
            bool dash = c == '-';
            bool under = c == '_';
            bool dot = c == '.';
            bool end = m_index > m_text.size();
            if (t.c < 0)
            {
                if (space)
                {
                    continue;
                }
                else if (hash || end)
                {
                    return true;
                }

                t.c = m_index;
                if (simple)
                {
                    t.val = m_text.substr(t.c - 1, 1);
                    switch (c)
                    {
                    case '(':
                        t.type = TokenType::OPEN_PAR;
                        return true;
                    case ')':
                        t.type = TokenType::CLOSED_PAR;
                        return true;
                    case '>':
                        t.type = TokenType::ARROW;
                        return true;
                    case ',':
                        t.type = TokenType::COMMA;
                        return true;
                    default:
                        printError(m_path, m_number, t.c, "Invalid token, this shouldn't be happening");
                        return false;
                    }
                }
                else if (under || letter)
                {
                    t.type = TokenType::STRING;
                    continue;
                }
                else if (digit || dash)
                {
                    t.type = TokenType::INT;
                    continue;
                }
                else if (dot)
                {
                    t.type = TokenType::FLOAT;
                    continue;
                }
            }
            else
            {
                bool terminating = space || simple || hash || end;
                if (terminating)
                {
                    std::string_view token = m_text.substr(t.c - 1, m_index - t.c);
                    --m_index;
                    switch (t.type)
                    {
                    case TokenType::STRING:
                        t.val = token;
                        return true;
                    case TokenType::INT:
                    {
                        int i;
                        auto res = std::from_chars(token.data(), token.data() + token.size(), i);
                        if (res.ec == std::errc::invalid_argument)
                        {
                            printError(m_path, m_number, t.c, "Invalid syntax, token couldn't be converted to integer");
                            return false;
                        }
                        t.val = i;
                        return true;
                    }
                    case TokenType::FLOAT:
                    {
                        int f;
                        auto res = std::from_chars(token.data(), token.data() + token.size(), f);
                        if (res.ec == std::errc::invalid_argument)
                        {
                            printError(
                                m_path, m_number, t.c, "Invalid syntax, token couldn't be converted to floating-point");
                            return false;
                        }
                        t.val = f;
                        return true;
                    }
                    default:
                        printError(m_path, m_number, t.c, "Invalid token, this shouldn't be happening");
                        return false;
                    }
                }
                else if (dash)
                {
                    printError(
                        m_path, m_number, t.c, "Invalid syntax, dashes can only be found at the beginning of numbers");
                    return false;
                }
                else if (t.type != TokenType::STRING && letter)
                {
                    printError(m_path, m_number, t.c, "Invalid syntax, numbers can't contain letters");
                    return false;
                }
                else if (t.type != TokenType::STRING && under)
                {
                    printError(m_path, m_number, t.c, "Invalid syntax, numbers can't contain underscores");
                    return false;
                }
                else if (dot)
                {
                    if (t.type == TokenType::INT)
                    {
                        t.type = TokenType::FLOAT;
                        continue;
                    }
                    else if (t.type == TokenType::FLOAT)
                    {
                        printError(
                            m_path, m_number, t.c, "Invalid syntax, numbers can't contain more than one dot character");
                        return false;
                    }
                }
            }
        } while (m_index <= m_text.size());
        return true;
    }

    bool expect(const std::vector<TokenType>& ts) noexcept
    {
        if (ts.size() > m_tokens.size() - m_index)
        {
            return false;
        }
        int i = m_index;
        for (auto t : ts)
        {
            if (t != m_tokens[i++].type)
            {
                return false;
            }
        }
        return true;
    }

    std::string_view m_path;
    std::string_view m_text;
    const int m_number;
    std::vector<Token> m_tokens{};
    int m_index{};
};

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

bool FilterPipeline::createComplexPipeline(const std::string& path) noexcept
{
    std::ifstream f(path);
    if (!f)
    {
        printError(path, 0, 0, "Could not open file");
        return {};
    }

    std::vector<TexturePlaceholder> texPlaceholders{};
    texPlaceholders.push_back(TexturePlaceholder{.name = "input", .createdAt = -1, .tex = 0});

    int line{};
    std::string text;
    while (std::getline(f, text))
    {
        LineInfo info{};
        {
            LineParser parser(path, text, ++line);
            if (!parser.parse(info))
            {
                return false;
            }
        }
        // comment line
        if (!info.valid())
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
                filterSlot.inputFilterSlots.push_back(texIndex);
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
            if (m_filterInstances.find(filterSlot.type) == m_filterInstances.end())
            {
                m_filterInstances[filterSlot.type] = std::make_unique<FilterInstance>(
                    "/home/victordadaciu/workspace/flint/compute/" + filterName + ".comp.spv");
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
                m_filterSlots.push_back(filterSlot);
                filterSlot.inputFilterSlots[0] = filterSlot.outputTexture;
            }
        }
        filterSlot.outputTexture = texPlaceholders.size();
        texPlaceholders.push_back(TexturePlaceholder{.name = outputString});
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
                       "Ill-formed fpl file, there must be at least one filter operation which only "
                       "has the keyword "
                       "'input' as inputs");
            return false;
        }
    }

    // refer to filter slot not tex placeholder
    for (int i = 0; i < m_filterSlots.size(); ++i)
    {
        texPlaceholders[m_filterSlots[i].outputTexture].createdAt = i;
        for (auto& input : m_filterSlots[i].inputFilterSlots)
        {
            input = input == 0 ? -1 : texPlaceholders[input].createdAt;
        }
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
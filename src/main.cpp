#include "FilterPipeline.h"
#include "FilterPipelineFileParser.h"
#include "FilterUtils.h"
#include "StagingBuffer.h"
#include "SubmissionStack.h"
#include "Texture.h"
#include "VkContext.h"

#include <cmdparser.hpp>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

static bool applyFilters(const cli::Parser& args) noexcept
{
    flint::FilterPipeline pipeline(args);
    if (!pipeline.valid())
    {
        pipeline.cleanup();
        return false;
    }
    unsigned char* raw = flint::loadImage(args.get<std::string>("i"));
    if (!raw)
    {
        return false;
    }
    flint::stagingBuffer.createFromRawImage(raw);
    std::vector<flint::Texture> texes(pipeline.uniqueTexturesCount());
    for (const auto& tex : texes)
    {
        if (!tex.valid())
        {
            pipeline.cleanup();
            for (auto& tex : texes)
            {
                tex.cleanup();
            }
            return false;
        }
    }
    flint::SubmissionStack submissions{};
    if (!pipeline.record(texes, submissions))
    {
        pipeline.cleanup();
        for (auto& tex : texes)
        {
            tex.cleanup();
        }
        submissions.clear();
    }
    std::vector<VkSubmitInfo> infos(submissions.size());
    std::vector<VkTimelineSemaphoreSubmitInfo> timelineInfos(submissions.size());
    uint64_t signalValue = 1;
    for (int i = 0; i < submissions.size(); ++i)
    {
        int waitCount = submissions[i].prereqs.size();
        submissions[i].waitValues.resize(waitCount, 1);

        timelineInfos[i].sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfos[i].signalSemaphoreValueCount = 1;
        timelineInfos[i].pSignalSemaphoreValues = &signalValue;
        timelineInfos[i].waitSemaphoreValueCount = waitCount;
        timelineInfos[i].pWaitSemaphoreValues = submissions[i].waitValues.data();

        infos[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        infos[i].commandBufferCount = 1;
        infos[i].pCommandBuffers = &submissions[i].commandBuffer;
        infos[i].signalSemaphoreCount = 1;
        infos[i].pSignalSemaphores = &submissions[i].signal;

        infos[i].waitSemaphoreCount = waitCount;
        submissions[i].waits.resize(waitCount);
        submissions[i].dstMasks.resize(waitCount);
        for (int wait = 0; wait < waitCount; ++wait)
        {
            submissions[i].waits[wait] = submissions[submissions[i].prereqs[wait]].signal;
            submissions[i].dstMasks[wait] = submissions[submissions[i].prereqs[wait]].stage;
        }
        infos[i].pWaitSemaphores = submissions[i].waits.data();
        infos[i].pWaitDstStageMask = submissions[i].dstMasks.data();
        infos[i].pNext = &timelineInfos[i];
    }

    VkFence fence{};
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(flint::ctx->device, &fenceInfo, nullptr, &fence);

    if (VK_FAILED(vkQueueSubmit(flint::ctx->queue, infos.size(), infos.data(), fence)))
    {
        std::cout << "Failed to run filter stages\n";
        pipeline.cleanup();
        for (auto& tex : texes)
        {
            tex.cleanup();
        }
        submissions.clear();
        return false;
    }

    vkWaitForFences(flint::ctx->device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(flint::ctx->device, fence, nullptr);

    flint::writeImage("/home/victordadaciu/workspace/flint/images/result.jpg");
    flint::stagingBuffer.cleanup();
    pipeline.cleanup();
    for (auto& tex : texes)
    {
        tex.cleanup();
    }
    submissions.clear();
    return true;
}

static bool parse(cli::Parser& args) noexcept
{
    args.set_optional<std::string>("i", "image", "", "Valid path to an image");
    args.set_optional<std::string>("f", "filter", "", "Valid filter name");
    args.run_and_exit_if_error();

    if (args.get<std::string>("i").empty())
    {
        std::cout << "No image path provided\n";
        return false;
    }
    if (flint::toFilterType.find(args.get<std::string>("f")) == flint::toFilterType.end())
    {
        std::cout << "Valid filter was not provided\n";
        return false;
    }
    return true;
}

static void printToken(const flint::Token& token) noexcept
{
    switch (token.type)
    {
    case flint::TokenType::ARROW:
        std::cout << ">\n";
        break;
    case flint::TokenType::OPEN_PAR:
        std::cout << "(\n";
        break;
    case flint::TokenType::CLOSED_PAR:
        std::cout << ")\n";
        break;
    case flint::TokenType::COMMA:
        std::cout << ",\n";
        break;
    case flint::TokenType::INPUT:
        std::cout << "input\n";
        break;
    case flint::TokenType::OUTPUT:
        std::cout << "output\n";
        break;
    case flint::TokenType::STRING:
        std::cout << "STRING\n";
        break;
    case flint::TokenType::FLOAT:
        std::cout << "FLOAT\n";
        break;
    case flint::TokenType::INT:
        std::cout << "INT\n";
        break;
    default:
    }
}

int main(int argc, const char* argv[])
{
    flint::FilterPipelineFileParser fileParser("/home/victordadaciu/workspace/flint/pipelines/test.fpl");
    std::vector<flint::Token> tokens(fileParser.parse());
    for (const auto& token : tokens)
    {
        printToken(token);
    }
    return 0;

    cli::Parser args(argc, argv);
    if (!parse(args))
    {
        std::cout << "\nflint failed to parse arguments\n";
        return 1;
    }

    if (!flint::init(args))
    {
        std::cout << "\nflint failed to initialize vulkan\n";
        return 1;
    }

    if (!applyFilters(args))
    {
        std::cout << "\nflint failed while applying requested filters\n";
        return 1;
    }

    flint::cleanup();
    return 0;
}
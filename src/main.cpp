#include "FilterPipeline.h"
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
        return false;
    }
    unsigned char* raw = flint::loadImage(args.get<std::string>("i"));
    if (!raw || !flint::stagingBuffer.createFromRawImage(raw))
    {
        return false;
    }
    std::vector<flint::Texture> texes(pipeline.texCount());
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

static bool parseArgs(cli::Parser& args) noexcept
{
    // TODO remove default values
    args.set_optional<std::string>(
        "i", "image", "/home/victordadaciu/workspace/flint/images/vault_boy.jpg", "Valid path to an image");
    args.set_optional<std::string>("f", "filter", "", "Valid filter name");
    args.set_optional<uint32_t>("radius", "radius", 3, "Parameter");
    args.run_and_exit_if_error();

    if (args.get<std::string>("i").empty())
    {
        std::cout << "No image path provided\n";
        return false;
    }
    return true;
}

int main(int argc, const char* argv[])
{
    cli::Parser args(argc, argv);
    if (!parseArgs(args))
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
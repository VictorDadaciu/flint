#include "Args.h"
#include "FilterPipeline.h"
#include "StagingBuffer.h"
#include "SubmissionStack.h"
#include "Texture.h"
#include "VkContext.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <vulkan/vulkan_core.h>

static bool applyFilters(const flint::Args& args) noexcept
{
    flint::FilterPipeline pipeline(args);
    if (!pipeline.valid())
    {
        pipeline.cleanup();
        return false;
    }
    flint::stagingBuffer.createFromRawImage(flint::loadImage(args.path));
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

int main(int argc, const char* argv[])
{
    flint::Args args;
    if (!flint::parseArgs(argc, argv, args))
    {
        std::cout << "\nflint failed while parsing arguments\n";
        return 1;
    }

    if (!flint::init(args))
    {
        std::cout << "flint failed to initialize vulkan\n";
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
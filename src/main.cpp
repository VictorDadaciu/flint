#include "Error.h"
#include "FilterPipeline.h"
#include "StagingBuffer.h"
#include "SubmissionStack.h"
#include "Texture.h"
#include "VkContext.h"

#include <cmdparser.hpp>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

static void applyFilters(const cli::Parser& args) noexcept
{
    flint::FilterPipeline pipeline(args);
    unsigned char* raw = flint::loadImage(args.get<std::string>("i"));
    if (!raw || !flint::stagingBuffer.createFromRawImage(raw))
    {
        flint::fail("Failed to open image file '" + args.get<std::string>("i") + "'");
    }
    std::vector<flint::Texture> texes(pipeline.texCount());

    flint::SubmissionStack submissions{};
    pipeline.record(texes, submissions);

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
        flint::fail("Failed to run filter stages");
    }

    vkWaitForFences(flint::ctx->device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(flint::ctx->device, fence, nullptr);

    flint::writeImage("/home/victordadaciu/workspace/flint/images/result.jpg");
}

[[nodiscard]] static cli::Parser parseArgs(int argc, const char* argv[]) noexcept
{
    // TODO remove default values
    cli::Parser args(argc, argv);
    args.set_optional<std::string>(
        "i", "image", "/home/victordadaciu/workspace/flint/images/vault_boy.jpg", "Valid path to an image");
    args.set_optional<std::string>("f", "filter", "", "Valid filter name");
    args.set_optional<uint32_t>("radius", "radius", 3, "Parameter");
    args.run_and_exit_if_error();

    if (args.get<std::string>("i").empty())
    {
        flint::fail("No image path provided");
    }
    if (args.get<std::string>("f").empty())
    {
        flint::fail("No filter or filter path provided");
    }
    return args;
}

int main(int argc, const char* argv[])
{
    cli::Parser args = parseArgs(argc, argv);
    flint::initVk(args);
    applyFilters(args);
    flint::stagingBuffer.cleanup();
    flint::cleanupVk();
    return 0;
}
#include "Args.h"
#include "FilterPipeline.h"
#include "QLog.h"
#include "StagingBuffer.h"
#include "SubmissionStack.h"
#include "Texture.h"
#include "Utils.h"
#include "VkContext.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

static std::filesystem::path chooseOutputPath(const flint::Args& args) noexcept
{
    std::filesystem::path outputPath{args.outputPath};
    bool isDir = !outputPath.has_extension();

    std::filesystem::path dirPath = isDir ? outputPath : outputPath.parent_path();
    if (!std::filesystem::exists(dirPath) && !std::filesystem::create_directories(dirPath))
    {
        flint::fail("Could not create directories requested by output path arg " + dirPath.string());
    }

    if (isDir)
    {
        outputPath /= args.inputPath.stem().string() +
                      '_' +
                      args.filterPath.stem().string() +
                      args.inputPath.extension().string();
    }

    if (args.noOverwrite)
    {
        std::string baseStem{outputPath.stem().string()};
        int index = 1;
        while (std::filesystem::exists(outputPath))
        {
            outputPath.replace_filename(baseStem + std::to_string(index++) + outputPath.extension().string());
        }
    }
    return outputPath;
}

static void applyFilters(const flint::Args& args) noexcept
{
    flint::FilterPipeline pipeline(args);
    unsigned char* raw = flint::loadImage(args.inputPath);
    if (!raw)
    {
        flint::fail("Failed to open image file " + args.inputPath.string());
    }
    flint::stagingBuffer.createFromRawImage(raw);

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

    flint::writeImage(chooseOutputPath(args));
}

int main(int argc, const char* argv[])
{
    qlog::init();
    qlog::set_name("flint");
#ifndef NDEBUG
    qlog::set_log_level(qlog::LogLevel::DEBUG);
#endif
    const auto args = flint::args::parse(argc, argv);

    flint::initVk();
    applyFilters(args);
    flint::stagingBuffer.cleanup();
    flint::cleanupVk();
    return 0;
}
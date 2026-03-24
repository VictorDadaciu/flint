#include "Args.h"
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
#include <filesystem>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

static std::filesystem::path chooseOutputPath(const cli::Parser& args) noexcept
{
    std::string outputString = args.get<std::string>("o");
    std::filesystem::path outputPath{outputString};
    bool isDir = !outputPath.has_extension();

    std::filesystem::path dirPath = isDir ? outputPath : outputPath.parent_path();
    if (!outputString.empty() && !(std::filesystem::exists(dirPath) || std::filesystem::create_directories(dirPath)))
    {
        flint::fail("Could not create directories requested by output path arg " + outputPath.string());
    }

    if (outputString.empty() || isDir)
    {
        std::filesystem::path inputPath = args.get<std::string>("i");
        if (outputString.empty())
        {
            outputPath = inputPath;
        }
        std::string filterName = args.get<std::string>("f");
        if (filterName.ends_with(".fpl"))
        {
            filterName = std::filesystem::path(filterName).stem().string();
        }

        std::string newFilename = inputPath.stem().string() + '_' + filterName + inputPath.extension().string();
        // case when dir is given without trailing slash on linux, the "stem" is treated as both a directory and a
        // filename for some reason
        if (std::filesystem::is_directory(outputPath) && outputPath.has_stem())
        {
            outputPath /= newFilename;
        }
        else
        {
            outputPath.replace_filename(newFilename);
        }
    }

    if (args.get<bool>("no-overwrite"))
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

    flint::writeImage(chooseOutputPath(args));
}

[[nodiscard]] static cli::Parser parseArgs(int argc, const char* argv[]) noexcept
{
    // TODO remove default values
    cli::Parser args(argc, argv);
    args.set_optional<std::string>(
        "i", "image", "/home/victordadaciu/workspace/flint/images/vault_boy.jpg", "Valid path to an image");
    args.set_optional<std::string>("f", "filter", "", "Valid filter name");
    args.set_optional<std::string>("o", "output", "", "Valid output path");
    args.set_optional<bool>(
        "no-overwrite", "no-overwrite", false, "Do not overwrite existing output file with same name");
    args.set_optional<uint32_t>("radius", "radius", 3, "Parameter");
    args.run_and_exit_if_error();

    if (args.get<std::string>("i").empty())
    {
        flint::fail("No image path provided");
    }
    std::string filter = args.get<std::string>("f");
    if (filter.empty() || (filter.ends_with(".fpl") && !std::filesystem::exists(filter)))
    {
        flint::fail("Invalid filter or filter path provided");
    }
    return args;
}

int main(int argc, const char* argv[])
{
    const auto myArgs = flint::args::parse(argc, argv);

    cli::Parser args = parseArgs(argc, argv);
    flint::initVk(args);
    applyFilters(args);
    flint::stagingBuffer.cleanup();
    flint::cleanupVk();
    return 0;
}
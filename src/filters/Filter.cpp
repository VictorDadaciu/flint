#include "filters/Filter.h"

#include "StagingBuffer.h"
#include "Texture.h"
#include "VkContext.h"
#include "filters/FlipAll.h"
#include "filters/FlipHorizontal.h"
#include "filters/FlipVertical.h"
#include "stages/FilterStage.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <type_traits>
#include <unistd.h>
#include <vector>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

namespace flint::detail
{
bool Filter::run()
{
    std::vector<VkSubmitInfo> infos{};
    for (int i = 0; i < m_stages.size(); ++i)
    {
        const std::vector<VkSubmitInfo>& submitInfos{m_stages[i]->submitInfos()};
        infos.insert(infos.end(), submitInfos.cbegin(), submitInfos.cend());
    }

    VkFence fence{};
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(vulkan::ctx->device, &fenceInfo, nullptr, &fence);

    if (VK_FAILED(vkQueueSubmit(vulkan::ctx->queue, infos.size(), infos.data(), fence)))
    {
        std::cout << "Failed to run filter stages\n";
        cleanup();
        return false;
    }

    vkWaitForFences(vulkan::ctx->device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(vulkan::ctx->device, fence, nullptr);
    cleanup();

    unsigned char* raw = vulkan::stagingBuffer.getAsRawImage();
    stbi_write_jpg("/home/victordadaciu/workspace/flint/images/result.jpg",
                   vulkan::imageMetadata.width,
                   vulkan::imageMetadata.height,
                   4,
                   raw,
                   100);
    stbi_image_free(raw);
    vulkan::stagingBuffer.cleanup();
    return true;
}

template<typename F>
struct FilterImpl
{
    static bool apply(const Args& args) noexcept
    {
        static_assert(std::is_convertible<F*, Filter*>::value, "Type provided is not valid filter");
        std::unique_ptr<F> filter = std::make_unique<F>(args);
        bool ret = filter->valid() && filter->run();
        std::cout << (ret ? "Filter(s) applied successfully\n" : "Failed to apply filters\n");
        return ret;
    }
};
} // namespace flint::detail

namespace flint
{
bool applyFilters(const Args& args) noexcept
{
    switch (args.filterType)
    {
    case FilterType::identity:
        std::cout << "Identity selected\n";
        break;
    case FilterType::flip_h:
        std::cout << "Flip horizontal selected\n";
        return detail::FilterImpl<FlipHorizontal>::apply(std::move(args));
    case FilterType::flip_v:
        std::cout << "Flip vertical selected\n";
        return detail::FilterImpl<FlipVertical>::apply(std::move(args));
    case FilterType::flip_all:
        std::cout << "Flip all selected\n";
        return detail::FilterImpl<FlipAll>::apply(std::move(args));
    case FilterType::kuwahara:
        std::cout << "Kuwahara selected\n";
        break;
    default:
        std::cout << "Unkown filter selected, should never reach\n";
        return false;
    }
    return true;
}
} // namespace flint
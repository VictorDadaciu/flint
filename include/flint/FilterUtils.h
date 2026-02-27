#pragma once

#include <vulkan/vulkan.h>

namespace flint
{
#define FILTER_ENUM(v1, v2, v3, v4, v5, v6)                                                                            \
    enum class FilterType                                                                                              \
    {                                                                                                                  \
        v1,                                                                                                            \
        v2,                                                                                                            \
        v3,                                                                                                            \
        v4,                                                                                                            \
        v5,                                                                                                            \
        v6,                                                                                                            \
        count                                                                                                          \
    };                                                                                                                 \
    static const char* filterStrings[] = {                                                                             \
        #v1,                                                                                                           \
        #v2,                                                                                                           \
        #v3,                                                                                                           \
        #v4,                                                                                                           \
        #v5,                                                                                                           \
        #v6,                                                                                                           \
    };

FILTER_ENUM(identity, flip_h, flip_v, flip_all, sobel, kuwahara);
} // namespace flint
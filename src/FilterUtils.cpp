#include "FilterUtils.h"

#include <string>
#include <unordered_map>

namespace flint
{
const std::unordered_map<std::string, FilterType> toFilterType = {
    {"flip_h", FilterType::flip_h},
    {"flip_v", FilterType::flip_v},
    {"flip_all", FilterType::flip_all},
    {"sobel", FilterType::sobel},
    {"box_blur", FilterType::box_blur},
};
} // namespace flint
#pragma once

#include <string>
#include <unordered_map>

namespace flint
{
enum class FilterType
{
    flip_h,
    flip_v,
    flip_all,
    sobel,
    box_blur,
    count
};

extern const std::unordered_map<std::string, FilterType> toFilterType;
} // namespace flint
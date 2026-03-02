#pragma once

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

const char* filterTypeToString(FilterType) noexcept;
} // namespace flint
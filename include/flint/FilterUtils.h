#pragma once

namespace flint
{
enum class FilterType
{
    flip_h,
    flip_v,
    flip_all,
    sobel,
    count
};
static const char* filterStrings[] = {"flip_h", "flip_v", "flip_all", "sobel"};

const char* filterTypeToString(FilterType) noexcept;
} // namespace flint
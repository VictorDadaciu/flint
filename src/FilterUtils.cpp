#include "FilterUtils.h"

namespace flint
{
const char* filterTypeToString(FilterType type) noexcept
{
    switch (type)
    {
    case FilterType::flip_h:
        return "flip_h";
    case FilterType::flip_v:
        return "flip_v";
    case FilterType::flip_all:
        return "flip_all";
    case FilterType::sobel:
        return "sobel";
    default:
        return "";
    }
}
} // namespace flint
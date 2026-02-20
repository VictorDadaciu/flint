#pragma once

#include <cstddef>

namespace flint
{
enum class FilterType
{
    IDENTITY = 0,
    KUWAHARA = 1,
    COUNT = 2
};
static const char* filterStrings[(size_t)FilterType::COUNT] = {"identity", "kuwahara"};
} // namespace flint
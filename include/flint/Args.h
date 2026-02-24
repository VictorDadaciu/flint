#pragma once

#include "FilterUtils.h"

namespace flint
{
struct Args
{
    FilterType filterType = FilterType::count;
    const char* path{};
};

bool parseArgs(int, const char**, Args&);
} // namespace flint
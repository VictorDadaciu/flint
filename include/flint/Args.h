#pragma once

#include "FilterUtils.h"
#include "Texture.h"

namespace flint
{
struct Args
{
    FilterType filterType = FilterType::COUNT;
    RawImage imageData{};
};

bool parseArgs(int, const char**, Args&);
} // namespace flint
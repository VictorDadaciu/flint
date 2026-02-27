#pragma once

#include "filters/Filter.h"

namespace flint
{
class Sobel final : public detail::Filter
{
public:
    Sobel(const Args&) noexcept;
};
} // namespace flint
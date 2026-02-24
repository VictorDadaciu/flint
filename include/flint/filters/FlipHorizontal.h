#pragma once

#include "filters/Filter.h"

namespace flint
{
class FlipHorizontal final : public detail::Filter
{
public:
    FlipHorizontal(const Args&) noexcept;
};
} // namespace flint
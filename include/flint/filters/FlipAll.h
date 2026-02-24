#pragma once

#include "filters/Filter.h"

namespace flint
{
class FlipAll final : public detail::Filter
{
public:
    FlipAll(const Args&) noexcept;
};
} // namespace flint
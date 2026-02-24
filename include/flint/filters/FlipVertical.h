#pragma once

#include "filters/Filter.h"

namespace flint
{
class FlipVertical final : public detail::Filter
{
public:
    FlipVertical(const Args&) noexcept;
};
} // namespace flint
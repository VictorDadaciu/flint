#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <variant>

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

extern const std::unordered_map<std::string, FilterType> toFilterType;
extern const std::unordered_map<FilterType, std::string> toFilterName;

using Parameter = std::variant<uint32_t, float>;
using ParameterMap = std::map<std::string, Parameter>;

namespace utils
{
    int parameterCount(FilterType) noexcept;
    ParameterMap parameterMap(FilterType) noexcept;
    bool validateMap(FilterType, const ParameterMap&) noexcept;
} // namespace utils
} // namespace flint
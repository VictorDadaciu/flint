#include "FilterUtils.h"

#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace flint
{
const std::unordered_map<std::string, FilterType> toFilterType = {
    {"flip_h", FilterType::flip_h},
    {"flip_v", FilterType::flip_v},
    {"flip_all", FilterType::flip_all},
    {"sobel", FilterType::sobel},
    {"box_blur", FilterType::box_blur},
};
const std::unordered_map<FilterType, std::string> toFilterName = {
    {FilterType::flip_h, "flip_h"},
    {FilterType::flip_v, "flip_v"},
    {FilterType::flip_all, "flip_all"},
    {FilterType::sobel, "sobel"},
    {FilterType::box_blur, "box_blur"},
};

namespace utils
{
    constexpr std::string PARAM_RADIUS = "radius";

    template<typename T>
    bool validateParameter(const std::string& key, const ParameterMap& map, T& ret) noexcept
    {
        static_assert(std::is_same_v<T, uint32_t> || std::is_same_v<T, float>,
                      "Only uint32_t or floats are allows as parameters");

        auto it = map.find(key);
        if (it == map.end() || !std::holds_alternative<T>(it->second))
        {
            return false;
        }
        ret = std::get<T>(it->second);
        return true;
    }

    bool validateEmptyMap(const ParameterMap& map) noexcept
    {
        return map.empty();
    }

    bool validateBoxBlurMap(const ParameterMap& map) noexcept
    {
        if (map.size() != 1)
        {
            return false;
        }

        uint32_t radius{};
        if (!validateParameter(PARAM_RADIUS, map, radius))
        {
            return false;
        }
        return radius >= 3 && radius % 2 == 1;
    }

    bool validateMap(FilterType type, const ParameterMap& map) noexcept
    {
        switch (type)
        {
        case FilterType::box_blur:
            return validateBoxBlurMap(map);
        default:
            return validateEmptyMap(map);
        }
    }

    ParameterMap parameterMap(FilterType type) noexcept
    {
        auto map = ParameterMap{};
        switch (type)
        {
        case FilterType::box_blur:
            map[PARAM_RADIUS] = 3u;
            break;
        default:
        }
        return map;
    }

    // TODO this sucks so bad, need something else
    int parameterCount(FilterType type) noexcept
    {
        return parameterMap(type).size();
    }
} // namespace utils
} // namespace flint
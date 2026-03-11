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

    bool mapAsData(FilterType type, const ParameterMap& map, int& size, void** data) noexcept
    {
        if (!validateMap(type, map))
        {
            return false;
        }

        size = map.size() * 4;
        *data = static_cast<void*>(new uint32_t[map.size()]);

        int i = 0;
        for (const auto& param : map)
        {
            if (std::holds_alternative<float>(param.second))
            {
                static_cast<float*>(*data)[i] = std::get<float>(param.second);
            }
            else
            {
                static_cast<uint32_t*>(*data)[i] = std::get<uint32_t>(param.second);
            }
            ++i;
        }
        return true;
    }
} // namespace utils
} // namespace flint
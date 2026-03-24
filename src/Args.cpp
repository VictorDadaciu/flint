#include "Args.h"

#include "Error.h"

#include <charconv>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace flint::args
{
[[noreturn]] static void uniqueError(int index)
{
    fail("Invalid arg #" + std::to_string(index) + ", can only be set once");
}

[[noreturn]] static void unexpectedEndError()
{
    fail("Unexpected end of arg list, last arg requires an input value");
}

[[noreturn]] static void requiresValueError(const std::string& arg)
{
    fail("Invalid args, setting " + arg + " is required");
}

Args parse(int argc, const char** argv) noexcept
{
    Args ret{};

    std::vector<std::string_view> args(argv + 1, argv + argc);
    for (int i = 0; i < args.size(); ++i)
    {
        auto& arg = args[i];
        if (arg == "-i" || arg == "--input")
        {
            if (!ret.inputPath.empty())
            {
                uniqueError(i);
            }
            if (++i >= args.size())
            {
                unexpectedEndError();
            }
            ret.inputPath = std::filesystem::path{args[i]};
        }
        else if (arg == "-f" || arg == "--filter")
        {
            if (!ret.filter.empty())
            {
                uniqueError(i);
            }
            if (++i >= args.size())
            {
                unexpectedEndError();
            }
            ret.filter = args[i];
        }
        else if (arg == "-o" || arg == "--output")
        {
            if (!ret.outputPath.empty())
            {
                uniqueError(i);
            }
            if (++i >= args.size())
            {
                unexpectedEndError();
            }
            ret.outputPath = std::filesystem::path{args[i]};
        }
        else if (arg == "--no-override")
        {
            if (ret.noOverwrite.has_value())
            {
                uniqueError(i);
            }
            ret.noOverwrite = true;
        }
        else if (arg.starts_with("--"))
        {
            arg.remove_prefix(2);
            std::string name = std::string(arg);
            std::cout << name << "\n";
            const auto& it = ret.params.find(name);
            if (it == ret.params.end())
            {
                fail("Invalid parameter passed as arg '--" + name + "'");
            }
            if (++i >= args.size())
            {
                unexpectedEndError();
            }
            auto value = args[i];
            bool includesDot = value.find('.') != value.npos;
            if (std::holds_alternative<uint32_t>(it->second) && !includesDot)
            {
                uint32_t u{};
                auto res = std::from_chars(value.data(), value.data() + value.size(), u);
                if (res.ec == std::errc::invalid_argument)
                {
                    fail("Unexpected value passed for arg '--" + name + "', expected unsigned integer");
                }
                ret.params[name] = u;
            }
            else if (std::holds_alternative<float>(it->second) && includesDot)
            {
                float f{};
                auto res = std::from_chars(value.data(), value.data() + value.size(), f);
                if (res.ec == std::errc::invalid_argument)
                {
                    fail("Unexpected value passed for arg '--" + name + "', expected float");
                }
                ret.params[name] = f;
            }
            else
            {
                fail("Unexpected value passed for arg '--" +
                     name +
                     "', expected " +
                     (std::holds_alternative<uint32_t>(it->second) ? "unsigned integer" : "float"));
            }
        }
        else
        {
            fail("Unrecognized argument #" +
                 std::to_string(i) +
                 ", you might not be setting up previous arguments correctly");
        }
    }
    if (ret.inputPath.empty())
    {
        requiresValueError("-i, --input");
    }
    if (ret.filter.empty())
    {
        requiresValueError("-f, --filter");
    }

    if (!ret.noOverwrite.has_value())
    {
        ret.noOverwrite = false;
    }
    if (ret.outputPath.empty())
    {
        ret.outputPath = ret.inputPath.parent_path();
    }
    return ret;
}
} // namespace flint::args
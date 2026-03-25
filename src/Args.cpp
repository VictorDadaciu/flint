#include "Args.h"

#include "Error.h"

#include <filesystem>
#include <string>
#include <string_view>
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

static void checkForHelpOrVersion(const std::vector<std::string_view>& args) noexcept
{
    if (args.empty() || (args.size() == 1 && (args[0] == "-h" || args[0] == "--help")))
    {
        // clang-format off
        std::cout << "flint, a command-line tool for image filtering.\n";
        std::cout << "Usage: flint (--input <path>) (--filter <path>) [options]\n";
        std::cout << "  Arguments can be provided in any order.\n\n";
        std::cout << "Arguments:\n";
        std::cout << "  -i,--input <path>   Path to an image file, supports jpg, png, bmp, tga, and hdr. REQUIRED\n";
        std::cout << "  -f,--filter <path>  Path to a fpl file. REQUIRED\n";
        std::cout << "  -o,--output <path>  Path to output the resulting input to. Default: path to the input's directory. OPTIONAL\n";
        std::cout << "  --no-overwrite      If not set, overwrite output file. Otherwise, create new output file with index at the end. Default: NOT set. OPTIONAL\n";
        // clang-format on
        std::exit(0);
    }

    if (args.size() == 1 && args[0] == "--version")
    {
        std::cout << "flint v1.0.0\n";
        std::exit(0);
    }
}

Args parse(int argc, const char** argv) noexcept
{
    Args ret{};

    std::vector<std::string_view> args(argv + 1, argv + argc);
    checkForHelpOrVersion(args);

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
            if (!ret.filterPath.empty())
            {
                uniqueError(i);
            }
            if (++i >= args.size())
            {
                unexpectedEndError();
            }
            ret.filterPath = std::filesystem::path(args[i]);
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
        else if (arg == "-h" || arg == "--help" || arg == "--version")
        {
            continue;
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
    if (ret.filterPath.empty())
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
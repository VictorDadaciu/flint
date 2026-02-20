#include "Args.h"

#include "FilterUtils.h"
#include "Texture.h"

#include <cstring>
#include <iostream>

namespace flint
{
#define POSSIBLE_OPTIONS_COUNT 3
static const char* possibleOptions[POSSIBLE_OPTIONS_COUNT] = {"--image", "--filter", "--help"};

static void printHelp()
{
    std::cout << "\nUsage: flint --image <path> --filter <filter>\n\n";
    std::cout << "  --image <path>: <path> must be a valid path to an image file\n";
    std::cout << "  --filter <filter>: <filter> must be a valid filter from the following:\n";
    for (const auto& filter : filterStrings)
    {
        std::cout << "      " << filter << "\n";
    }
}

static bool startsWith(const char* arg, const char* prefix) noexcept
{
    size_t n = strlen(prefix);
    if (strlen(arg) < n)
    {
        return false;
    }

    for (int i = 0; i < n; ++i)
    {
        if (arg[i] != prefix[i])
        {
            return false;
        }
    }
    return true;
}

static int optionIndex(const char* option) noexcept
{
    for (int i = 0; i < POSSIBLE_OPTIONS_COUNT; ++i)
    {
        if (strcmp(possibleOptions[i], option) == 0)
        {
            return i;
        }
    }
    return -1;
}

static FilterType parsefilterType(const char* arg) noexcept
{
    for (int i = 0; i < (size_t)FilterType::COUNT; ++i)
    {
        if (strcmp(filterStrings[i], arg) == 0)
        {
            return (FilterType)i;
        }
    }
    return FilterType::COUNT;
}

bool parseArgs(int argc, const char* argv[], Args& args)
{
    for (int i = 0; i < argc; ++i)
    {
        // if not option, skip
        if (startsWith(argv[i], "--"))
        {
            int option = optionIndex(argv[i]);
            const char* param = nullptr;
            // get parameter if option requires it
            switch (option)
            {
            case 0:
            case 1:
                if (++i < argc)
                {
                    param = argv[i];
                }
                else
                {
                    std::cout << "Option \"" << possibleOptions[option] << "\" is missing a parameter.\n";
                    return false;
                }
            default:
            }

            // now actually fill values
            switch (option)
            {
            case 0:
                args.imageData = RawImage(param);
                break;
            case 1:
                args.filterType = parsefilterType(param);
                break;
            case 2:
                printHelp();
                std::exit(0);
            default:
            }
        }
    }
    if (!args.imageData.valid())
    {
        std::cout << "Image path provided is not a valid image path.\n";
        return false;
    }
    if (args.filterType == FilterType::COUNT)
    {
        std::cout << "Filter type provided is not a valid filter. Run \"flint --help\" to see list of valid filters.\n";
        return false;
    }
    return true;
}
} // namespace flint
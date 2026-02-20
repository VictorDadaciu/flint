#include "Args.h"
#include "FilterUtils.h"
#include "VkContext.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace flint
{
static bool applyFilter(const Args& args)
{
    switch (args.filterType)
    {
    case FilterType::IDENTITY:
        std::cout << "Identity selected\n";
        break;
    case FilterType::KUWAHARA:
        std::cout << "Kuwahara selected\n";
        break;
    default:
        std::cout << "Unkown filter selected, should never reach\n";
        return false;
    }
    return true;
}
} // namespace flint

int main(int argc, const char* argv[])
{
    flint::Args args;
    if (!flint::parseArgs(argc, argv, args))
    {
        std::cout << "\nflint failed while parsing arguments\n";
        return 1;
    }

    if (!flint::vulkan::init(args))
    {
        std::cout << "flint failed to initialize vulkan\n";
        return 1;
    }

    if (!flint::applyFilter(args))
    {
        std::cout << "\nflint failed while applying requested filter\n";
        return 1;
    }

    flint::vulkan::cleanup();
    return 0;
}
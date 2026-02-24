#include "Args.h"
#include "VkContext.h"
#include "filters/Filter.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vulkan/vulkan_core.h>

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

    if (!flint::applyFilters(args))
    {
        std::cout << "\nflint failed while applying requested filters\n";
        return 1;
    }

    flint::vulkan::cleanup();
    return 0;
}
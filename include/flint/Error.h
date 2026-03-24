#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

namespace flint
{
inline void warn(std::string&& msg)
{
    std::cerr << "flint warning: " << msg << "\n";
}

[[noreturn]] inline void fail(std::string&& msg)
{
    std::cerr << "flint exited with error: " << msg << "\n";

    std::exit(1);
}
} // namespace flint
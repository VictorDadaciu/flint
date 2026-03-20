#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

namespace flint
{
inline void warn(std::string&& msg)
{
    std::cerr << "\nflint warning: " << msg << "\n";
}

inline void fail(std::string&& msg)
{
    std::cerr << "\nflint exited with error: " << msg << "\n";

    std::exit(1);
}
} // namespace flint
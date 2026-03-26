#pragma once

#include <QLog.h>
#include <filesystem>
#include <fstream>
#include <vector>

namespace flint
{
[[noreturn]] inline void fail(std::string&& msg)
{
    qlog::error("flint exited with error: " + msg);

    std::exit(1);
}

std::vector<char> readEntireFile(const std::filesystem::path&) noexcept;
uint32_t readInt(std::ifstream&) noexcept;
void combine(const std::string&, std::vector<uint32_t>&) noexcept;
std::string uncombine(std::ifstream&) noexcept;
} // namespace flint
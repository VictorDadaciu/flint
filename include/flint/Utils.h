#pragma once

#include <filesystem>
#include <fstream>
#include <vector>

namespace flint::utils
{
std::vector<char> readEntireFile(const std::filesystem::path&) noexcept;
uint32_t readInt(std::ifstream&) noexcept;
void combine(const std::string&, std::vector<uint32_t>&) noexcept;
std::string uncombine(std::ifstream&) noexcept;
} // namespace flint::utils
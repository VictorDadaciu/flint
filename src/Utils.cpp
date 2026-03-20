#include "Utils.h"

#include "Error.h"

#include <fstream>
#include <iostream>

namespace flint::utils
{
std::vector<char> readEntireFile(const std::filesystem::path& path) noexcept
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        warn("Failed to open file '" + path.string() + "' for read");
        return {};
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

uint32_t readInt(std::ifstream& f) noexcept
{
    uint32_t a{};
    f.read(reinterpret_cast<char*>(&a), sizeof(uint32_t));
    return a;
}

std::string uncombine(std::ifstream& f) noexcept
{
    std::vector<char> ret{};

    int size = utils::readInt(f);
    for (int i = 0; i < size / 4; ++i)
    {
        uint32_t code = utils::readInt(f);
        ret.push_back(static_cast<char>(((code & (0x000000ff << 24)) >> 24) & 0x000000ff));
        ret.push_back(static_cast<char>(((code & (0x000000ff << 16)) >> 16) & 0x000000ff));
        ret.push_back(static_cast<char>(((code & (0x000000ff << 8)) >> 8) & 0x000000ff));
        ret.push_back(static_cast<char>(code & 0x000000ff));
    }

    int howManyLeft = size % 4;
    if (howManyLeft == 0)
    {
        return std::string{ret.cbegin(), ret.cend()};
    }

    uint32_t rem = utils::readInt(f);
    for (int i = 0; i < howManyLeft; ++i)
    {
        int bits = 8 * (3 - i);
        ret.push_back(static_cast<char>((((rem & (0x000000ff << bits))) >> bits) & 0x000000ff));
    }

    return std::string{ret.cbegin(), ret.cend()};
}

void combine(const std::string& s, std::vector<uint32_t>& toWrite) noexcept
{
    toWrite.push_back(s.size());
    int i{};
    int rem = s.size() % 4;
    for (i = 0; i < s.size() - rem; i += 4)
    {
        toWrite.push_back((static_cast<uint32_t>(s[i + 0]) << 24) |
                          (static_cast<uint32_t>(s[i + 1]) << 16) |
                          (static_cast<uint32_t>(s[i + 2]) << 8) |
                          static_cast<uint32_t>(s[i + 3]));
    }

    if (rem == 0)
    {
        return;
    }

    uint32_t last{};
    int index = i;
    for (i = index; i < s.size(); ++i)
    {
        last |= static_cast<uint32_t>(s[i]) << (8 * (3 - i + index));
    }
    toWrite.push_back(last);
}

} // namespace flint::utils
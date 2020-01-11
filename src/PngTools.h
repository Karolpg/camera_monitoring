#pragma once

#include <cstdint>
#include <vector>

namespace PngTools {

bool writePngFile(const char *filename, uint32_t width, uint32_t height, uint32_t components, const uint8_t* rawData);
bool readPngFile(const char *filename, uint32_t& width, uint32_t& height, uint32_t& components, std::vector<uint8_t>& data);

} // namespace PngTools

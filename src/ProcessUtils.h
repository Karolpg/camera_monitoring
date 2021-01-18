#pragma once

#include <string>
#include <cstdint>

namespace ProcessUtils {

std::string memoryInfo();

uint64_t currentProcessSize();
std::string humanReadableSize(uint64_t size);

} // namespace ProcessUtils

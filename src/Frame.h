#pragma once

#include <vector>
#include <cstdint>
#include <chrono>

struct Frame
{
    uint64_t nr = 0;
    uint32_t bufferIdx = 0;
    std::chrono::steady_clock::time_point time;
    std::vector<uint8_t> data;
};

struct FrameDescr
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t components = 0;
};

#pragma once

#include <vector>
#include <cstdint>
#include <chrono>
#include "ImgUtils.h"

template <typename ComponentType>
struct Frame
{
    uint64_t nr = 0;
    uint32_t bufferIdx = 0;
    std::chrono::steady_clock::time_point time;
    std::vector<ComponentType> data;
};

using FrameU8 = Frame<uint8_t>;
using FrameF32 = Frame<float>;

struct FrameDescr
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t components = 0;
};

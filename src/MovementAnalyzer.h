#pragma once

#include "Frame.h"
#include <chrono>
#include <array>
#include <map>

class MovementAnalyzer
{
public:
    MovementAnalyzer();

    void feedAnalyzer(const FrameU8 &frame, const FrameDescr& descr);

    bool isMovementDetected() const { return m_movementDetected; }
private:

    void allocateMem();
    void scaleFrame(const FrameU8 &frame, const FrameDescr &descr, FrameU8 *outFrame);
    void analyzeMovement();
    void makeRegions();

    FrameU8 *m_baseFrame = nullptr;
    FrameU8 *m_nextFrame = nullptr;
    std::array<FrameU8, 2> m_cacheBase;
    std::array<FrameU16, 1> m_cache;

    FrameDescr m_descrOrg;
    FrameDescr m_descrBase;

    volatile bool m_movementDetected = false;
    std::chrono::time_point<std::chrono::steady_clock> m_firstFrameTime;

    static constexpr double TIME_BETWEEN_FRAMES = 0.3; // [s]
    static constexpr uint32_t PREFERED_SIZE = 512;
    static constexpr uint32_t REGION_THRESHOLD = 50*30;

    std::map<uint16_t, uint32_t> m_regions; // key = regionId, value = pixel count
};

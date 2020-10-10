#pragma once

#include "Frame.h"
#include <chrono>
#include <array>

class MovementAnalyzer
{
public:
    MovementAnalyzer();

    void feedAnalyzer(const FrameU8 &frame, const FrameDescr& descr);

    bool isMovementDetected() const { return m_movementDetected; }
private:
    using Pyramid = std::vector<FrameF32>;

    void allocateMem();
    void buildPyramid(const FrameU8 &frame, const FrameDescr &descr, Pyramid* pyramid);
    void analyzeMovement();

    std::array<Pyramid, 2> m_pyramidFrames;
    Pyramid* m_pyramidFirstFrame = nullptr;
    Pyramid* m_pyramidSecondFrame = nullptr;
    FrameDescr m_descr;

    bool m_movementDetected = false;
    std::chrono::time_point<std::chrono::steady_clock> m_firstFrameTime;

    const double TIME_BETWEEN_FRAMES = 0.3; // [s]
    const uint32_t PYRAMIND_START_SIZE = 512; // have to pow of 2
    const uint32_t PYRAMIND_END_SIZE = 32; // have to pow of 2
};

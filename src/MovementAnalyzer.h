//
// The MIT License (MIT)
//
// Copyright 2020 Karolpg
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to #use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR #COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#pragma once

#include "Frame.h"
#include <chrono>
#include <array>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class MovementAnalyzer
{
public:
    using OnMovementDetected = std::function<void(uint64_t frameNumber, uint32_t mainBufferIdx, void* ctx)>;

    MovementAnalyzer();
    ~MovementAnalyzer();

    void feedAnalyzer(const FrameU8 &frame, const FrameDescr& descr);

    void subscribeOnMovementDetected(OnMovementDetected notifyFunc, void* ctx = nullptr);
    void unsubscribeOnMovementDetected(OnMovementDetected notifyFunc, void* ctx = nullptr);
private:

    void allocateMem();
    void scaleFrame(const FrameU8 &frame, const FrameDescr &descr, FrameU8 *outFrame);
    void analyzeMovement();
    void makeRegions();
    void notifyAboutMovementDetected();

    FrameU8 *m_baseFrame = nullptr;
    FrameU8 *m_nextFrame = nullptr;
    std::array<FrameU8, 2> m_cacheBase;
    std::array<FrameU16, 1> m_cache;

    FrameDescr m_descrOrg;
    FrameDescr m_descrBase;

    std::chrono::time_point<std::chrono::steady_clock> m_firstFrameTime;

    static constexpr double TIME_BETWEEN_FRAMES = 0.3; // [s]
    static constexpr uint32_t PREFERED_SIZE = 512;
    static constexpr uint32_t REGION_THRESHOLD = 50*30;

    std::map<uint16_t, uint32_t> m_regions; // key = regionId, value = pixel count

    volatile bool m_threadIsRunning = true;
    volatile bool m_newTask = false;
    std::mutex m_waitForCalculationTaskMtx;
    std::condition_variable m_waitForCalculationTaskCv;
    std::thread m_calculationThread;

    std::mutex m_listenerMovementDetectedMutex;
    std::vector<std::tuple<void*, OnMovementDetected>> m_movementDetectedListener;
};

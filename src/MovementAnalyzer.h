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

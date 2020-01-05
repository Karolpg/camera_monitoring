#pragma once

#include <vector>
#include <cstdint>
#include <chrono>
#include <memory>

#include "Detector.h"

struct Frame
{
    uint64_t nr = 0;
    std::chrono::steady_clock::time_point time;
    std::vector<uint8_t> data;
};


class FrameControler
{
public:
    FrameControler() {}
    ~FrameControler() {}

    void setBufferParams(double duration, double cameraFps, uint32_t width, uint32_t height, uint32_t components);
    void setDetector(const std::shared_ptr<Detector>& detector) { m_detector = detector; }

    void addFrame(const uint8_t* data);

    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    uint32_t getComponents() const { return m_components; }

private:
    bool isFrameChanged(const Frame& f1, const Frame& f2);

    double m_cameraFps = 0.0;
    double m_frameTime = 0.0;

    std::vector<Frame> m_cyclicBuffer;

    uint64_t m_frameCtr = 0;

    // common data for every frame
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_components = 0;

    std::shared_ptr<Detector> m_detector;
};

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

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include "Frame.h"
#include "Config.h"

struct network;

struct DetectionBox
{
    float x, y, w, h;
};

struct DetectionResult
{
    uint32_t classId;
    const std::string& label;
    float probablity;
    DetectionBox box;
};

class Detector
{
public:
    struct Image {
        FrameU8 frame;
        FrameDescr descr;
    };

    Detector(const Config& cfg);
    ~Detector();

    ///
    /// \param data   - image pixels, data is copied to cache
    /// \param width  - image width
    /// \param height - image height
    /// \param components - image component of pixel, one component equal one byte, e.g. 1 - R, 2 - RG, 3 - RGB, 4 - RGBA
    ///                     darknet expects RGB so if input is different then last channel is copied or removed
    ///
    void setInput(const FrameU8& frame, const FrameDescr& descr);

    ///
    /// \return true - if find something, false - if find nothing
    ///
    bool detect();

    const std::vector<DetectionResult>& lastResults() const { return m_lastDetections; }
    const Image& getNetOutImg();
    const Image& getLabeledInImg();
    const Image& getInImg() const { return m_inImage; }

    void drawResults(std::vector<uint8_t>& data, uint32_t w, uint32_t h, uint32_t c);

protected:

    void drawResults(std::vector<uint8_t>& data, uint32_t w, uint32_t h, uint32_t c, uint32_t imgX, uint32_t imgY, uint32_t validAreaW, uint32_t validAreaH);
    void readLabels(const std::string& labelsFilePath, const std::string& expectedLabelsFilePath);
    void generateLabelsImg() const;

    network* m_net = nullptr;
    std::vector<float> m_netInput;

    float m_netThreshold = 0.1f;
    std::vector<std::string> m_labels;
    std::unordered_map<uint32_t, uint32_t> m_expectedLabelColors;

    bool m_outNetImageHasLabels;
    uint32_t m_netScaledImageX;
    uint32_t m_netScaledImageY;
    uint32_t m_netScaledImageW;
    uint32_t m_netScaledImageH;
    Image m_outNetImage;

    bool m_inImageHasLabels;
    Image m_inImage;

    std::vector<DetectionResult> m_lastDetections;
};

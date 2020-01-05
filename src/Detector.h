#pragma once

#include <string>
#include <vector>

struct network;

struct DetectionBox
{
    float x, y, w, h;
};

struct DetectionResult
{
    int classId;
    const std::string& label;
    float probablity;
    DetectionBox box;
};

class Detector
{
public:
    Detector(const std::string& netConfigFilePath,
             const std::string& weightsFilePath,
             const std::string& labelsFilePath,
             float netThreshold);
    ~Detector();
    bool detect(const uint8_t* data, uint32_t width, uint32_t height, uint32_t components);

    const std::vector<DetectionResult>& lastResults() const { return m_lastDetections; }
    const std::vector<uint8_t>& getNetOutImg(uint32_t& w, uint32_t& h, uint32_t& c);


protected:
    network* m_net = nullptr;
    std::vector<float> m_netInput;


    float m_netThreshold = 0.1f;
    std::vector<std::string> m_labels;
    std::vector<uint32_t> m_labelColors;

    bool m_outImageIsValid;
    uint32_t m_netScaledImageW;
    uint32_t m_netScaledImageH;
    std::vector<uint8_t> m_outImage;
    std::vector<DetectionResult> m_lastDetections;
};

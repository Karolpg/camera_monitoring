#include "FrameControler.h"

#include <algorithm>
#include <assert.h>
#include <chrono>
#include <cstring>
#include <string>
#include <iostream>
#include <thread>

#include <png.h>

static bool writePngFile(const char *filename, uint32_t width, uint32_t height, png_bytep rawData)
{
    FILE *fp = fopen(filename, "wb");
    if(!fp) return false;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) return false;

    png_infop info = png_create_info_struct(png);
    if (!info) return false;

    if (setjmp(png_jmpbuf(png))) return false;

    png_init_io(png, fp);

    // Output is 8bit depth, RGB format.
    png_set_IHDR(png,
                 info,
                 width, height,
                 8,
                 PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    // To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
    // Use png_set_filler().
    //png_set_filler(png, 0, PNG_FILLER_AFTER);

    if (!rawData) {
        png_destroy_write_struct(&png, &info);
        return false;
    }

    for(uint32_t h = 0; h < height; ++h) {
        png_write_row(png, &rawData[width*h*3]);
    }
    png_write_end(png, nullptr);

    fclose(fp);

    png_destroy_write_struct(&png, &info);
    return true;
}

static bool storeFrame(const Frame& frame, uint32_t width, uint32_t height)
{
    png_bytep raw = (png_bytep)frame.data.data();

    static const std::string path("/tmp/");
    std::string fileName = std::to_string(frame.nr);
    std::string pathFileName = path + fileName + ".png";
    if (!writePngFile(pathFileName.c_str(), width, height, raw)) {
        std::cerr << "Can't write png file!!!\n";
    }
}

void FrameControler::setBufferParams(double duration, double cameraFps, uint32_t width, uint32_t height, uint32_t components)
{
    assert(width);
    assert(height);
    assert(components);
    assert(duration > 0.0001);
    assert(cameraFps > 0.1);
    uint32_t frames = static_cast<uint32_t>(duration*cameraFps);
    frames = std::max(static_cast<uint32_t>(1), frames);
    m_cyclicBuffer.resize(frames);
    m_width = width;
    m_height = height;
    m_components = components;
    m_cameraFps = cameraFps;
    m_frameTime = 1.0 / m_cameraFps;

    for (Frame& frame : m_cyclicBuffer) {
        frame.data.resize(m_width*m_height*m_components);
    }
}

void FrameControler::addFrame(const uint8_t* data)
{
    assert(data);
    uint64_t prevFrameNr = m_frameCtr;
    uint32_t prevFrameInBuffer = static_cast<uint32_t>(prevFrameNr % m_cyclicBuffer.size());

    uint64_t frameNr = ++m_frameCtr;
    uint32_t frameInBuffer = static_cast<uint32_t>(frameNr % m_cyclicBuffer.size());
    m_cyclicBuffer[frameInBuffer].nr = frameNr;
    m_cyclicBuffer[frameInBuffer].time = std::chrono::steady_clock::now();
    std::memcpy(m_cyclicBuffer[frameInBuffer].data.data(), data, m_width*m_height*m_components);

    //std::chrono::duration<double> distanceBetweenFrames = m_cyclicBuffer[frameInBuffer].time - m_cyclicBuffer[prevFrameInBuffer].time;
    //if (std::abs(distanceBetweenFrames.count() - m_frameTime) > m_frameTime*0.01) {
    //    std::cout << "Warning! Frame provided too late: " << distanceBetweenFrames.count() - m_frameTime << "[s] fault. Distance: " << distanceBetweenFrames.count() << "[s] \n";
    //}

    if (m_detector && isFrameChanged(m_cyclicBuffer[frameInBuffer], m_cyclicBuffer[prevFrameInBuffer])) {
        //generalnie to ponizej jest do dupy
        //calosc bedzie dzialac jesli nie zmienia sie zmienne pomiędzy wywolaniami
        //narazie nic nie jest zabezpieczone, rozmiar sie nie zmieni wiec bedzie ok
        //doswiadczalnie czas detekcji przez siec jest mniejszy niz dlugosc bufora cyklicznego na moim PC
        volatile static bool runInThread = false;
        if (!runInThread) {
            runInThread = true;
            std::thread( [this, frameNr, frameInBuffer]
            {
                std::cout << "Detecting for: " << frameNr << "(" << frameInBuffer << ")\n";
                auto start = std::chrono::steady_clock::now();
                if (m_detector->detect(m_cyclicBuffer[frameInBuffer].data.data(), m_width, m_height, m_components)) {
                    std::chrono::duration<double> detectTime = std::chrono::steady_clock::now() - start;
                    std::cout << "Detection time: " << detectTime.count() << "[s]\n";

                    auto results = m_detector->lastResults();
                    for (const DetectionResult& dr : results) {
                        if (dr.probablity > 0.15f) {
                            //std::cout << "WARNING person detected!!!\n";

                            std::cout << "Detected: [" << dr.classId << "] " << dr.label << " " << dr.probablity
                                      << " (" << dr.box.x << "x" << dr.box.y << ", " << dr.box.w << "x" << dr.box.h <<  "\n";
                        }
                    }
                    uint32_t w = 0, h = 0, c = 0;
                    auto detectedOutImg = m_detector->getNetOutImg(w, h, c);
                    writePngFile((std::string("/tmp/detectionResult") + std::to_string(frameNr) + ".png").c_str(), w, h, detectedOutImg.data());
                    //writePngFile("/tmp/detectionResult.png", w, h, detectedOutImg.data());

                    //storeFrame(m_cyclicBuffer[frameInBuffer], m_width, m_height);
                }
                std::cout.flush();
                runInThread = false;
            }).detach();
        }
    }
}

bool FrameControler::isFrameChanged(const Frame& f1, const Frame& f2)
{
    const uint32_t PIXEL_COUNT_THRESHOLD = static_cast<uint32_t>(m_width*m_height*0.05);
    const uint32_t COMPONENT_THRESHOLD = 15;
    uint32_t componentDiff = 0;

    const uint32_t TIME_POS_X1 = 368;
    const uint32_t TIME_POS_X2 = 452;
    const uint32_t TIME_POS_Y1 = 21;
    const uint32_t TIME_POS_Y2 = 45;

    for (uint32_t c = 0; c < m_components; ++c) {
        for (uint32_t y = 0; y < m_height; ++y) {
            for (uint32_t x = 0; x < m_width; ++x) {
                if(x > TIME_POS_X1 && x < TIME_POS_X2 && y > TIME_POS_Y1 && y < TIME_POS_Y2) {
                    continue; // temporary solution to skip camera time
                }
                uint32_t idx = (y * m_width + x) * m_components + c;
                uint32_t localDiff = f1.data[idx] > f2.data[idx] ? f1.data[idx] - f2.data[idx] : f2.data[idx] - f1.data[idx];
                if (localDiff > COMPONENT_THRESHOLD) {
                    ++componentDiff;
                }
            }
        }
    }

    //std::vector<uint8_t> timePixels(m_components*(TIME_POS_X2-TIME_POS_X1)*(TIME_POS_Y2-TIME_POS_Y1));
    //for (uint32_t y = TIME_POS_Y1; y <TIME_POS_Y2; ++y) {
    //        uint32_t srcIdx = (y*m_width + TIME_POS_X1)*m_components;
    //        uint32_t dstIdx = (y-TIME_POS_Y1)*(TIME_POS_X2-TIME_POS_X1)*m_components;
    //        std::memcpy(&timePixels[dstIdx], &f1.data[srcIdx], (TIME_POS_X2-TIME_POS_X1)*m_components);
    //}
    //writePngFile("/tmp/timeTest.png", TIME_POS_X2-TIME_POS_X1, TIME_POS_Y2-TIME_POS_Y1, timePixels.data());

    uint32_t pixelDiff = componentDiff/m_components;
    std::cout << "Pixel diff: " << pixelDiff << "\n";

    return (pixelDiff > PIXEL_COUNT_THRESHOLD);
}



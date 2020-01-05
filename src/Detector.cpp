#include "Detector.h"
#include "ColorGenerator.h"
#include "Letters.h"

#include <darknet.h>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <cstring>
#include <array>
#include <algorithm>

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

Detector::Detector(const std::string& netConfigFilePath,
                   const std::string& weightsFilePath,
                   const std::string& labelsFilePath,
                   const std::string& expectedLabelsFilePath,
                   float netThreshold)
    : m_netThreshold(netThreshold)
{
    // I have no idea why someone assumed to provide file path as non const pointer!?
    m_net = load_network(const_cast<char*>(netConfigFilePath.c_str()), const_cast<char*>(weightsFilePath.c_str()), 0);
    if (!m_net) {
        std::cerr << "Can't load neural network with cfg file: " << netConfigFilePath << ", weights file: " << weightsFilePath << "\n";
        return;
    }
    set_batch_network(m_net, 1);
    srand(2222222);

    readLabels(labelsFilePath, expectedLabelsFilePath);

    assert(m_net->w * m_net->h * m_net->c > 0);

    m_netInput.resize(static_cast<size_t>(m_net->w * m_net->h * m_net->c), 0.0f);
    m_outNetImage.data.resize(static_cast<size_t>(m_net->w * m_net->h * m_net->c), 0);
    m_outNetImage.w = static_cast<uint32_t>(m_net->w);
    m_outNetImage.h = static_cast<uint32_t>(m_net->h);
    m_outNetImage.c = static_cast<uint32_t>(m_net->c);
}

Detector::~Detector()
{
    if (m_net) {
        free_network(m_net);
    }
}

float avarageColor(uint32_t x, uint32_t y, uint32_t c,
                       const uint8_t* data, uint32_t width, uint32_t height, uint32_t components,
                       double wAspect, double hAspect)
{
    assert(c <= components);
    if (wAspect >= 1.0) {
        if (hAspect >= 1.0) {
            uint32_t rawX = static_cast<uint32_t>(wAspect * x);
            uint32_t rawY = static_cast<uint32_t>(hAspect * y);
            uint32_t accColor = 0;

            uint32_t maskX = static_cast<uint32_t>(wAspect + 0.5);
            uint32_t maskY = static_cast<uint32_t>(hAspect + 0.5);
            uint32_t rawXEnd = std::min(rawX+maskX, width);
            uint32_t rawYEnd = std::min(rawY+maskY, height);
            uint32_t maskSize = (rawXEnd - rawX) * (rawYEnd - rawY);

            for (; rawY < rawYEnd; ++rawY) {
                for (; rawX < rawXEnd; ++rawX) {
                    accColor += data[(rawY*width+rawX)*components + c];
                }
            }
            return float(accColor) / maskSize / 255.0f;
        }
        else {
            assert(!"Not implemented yet!");
        }
    }
    else {
        assert(!"Not implemented yet!");
    }
    return 0.f;
}

void Detector::setInput(const uint8_t* data, uint32_t width, uint32_t height, uint32_t components)
{
    assert(data);
    assert(width);
    assert(height);
    assert(components);

    m_inImage.data.resize(width*height*components);
    std::memcpy(m_inImage.data.data(), data, width*height*components);
    m_inImage.w = width;
    m_inImage.h = height;
    m_inImage.c = components;
}

bool Detector::detect()
{
    m_lastDetections.clear();
    m_outNetImageHasLabels = false;
    m_inImageHasLabels = false;

    assert(m_inImage.data.size());

    if (!m_net) {
        assert(!"Network not created!\n");
        return false;
    }

    //keep the proportions
    const uint32_t w = static_cast<uint32_t>(m_net->w);
    const uint32_t h = static_cast<uint32_t>(m_net->h);
    uint32_t newW = m_inImage.w;
    uint32_t newH = m_inImage.h;
    if ((static_cast<float>(w)/m_inImage.w) < (static_cast<float>(h)/m_inImage.h)) {
        newW = w;
        newH = (m_inImage.h * w)/m_inImage.w;
    } else {
        newH = h;
        newW = (m_inImage.w * h)/m_inImage.h;
    }
    m_netScaledImageW = newW;
    m_netScaledImageH = newH;

    //rescale
    if (m_inImage.c >= static_cast<uint32_t>(m_net->c)) {
        const uint32_t c = static_cast<uint32_t>(m_net->c);

        double wAspect = static_cast<double>(m_inImage.w) / static_cast<double>(newW);
        double hAspect = static_cast<double>(m_inImage.h) / static_cast<double>(newH);

        for (uint32_t netComponent = 0; netComponent < c; ++netComponent) {
            for (uint32_t y = 0; y < newH; ++y) {
                for (uint32_t x = 0; x < newW; ++x) {
                    uint32_t idx = y*w + x + netComponent*w*h;
                    m_netInput[idx] = avarageColor(x, y, netComponent, m_inImage.data.data(), m_inImage.w, m_inImage.h, m_inImage.c, wAspect, hAspect);
                }
            }
            //clear right site
            if (w - newW > 0) {
                for (uint32_t y = 0; y < newH; ++y) {
                    uint32_t idx = y*w + newW + netComponent*w*h;
                    std::memset(&m_netInput[idx], 0, (w - newW)*sizeof(float));
                }
            }

            //clear bottom
            for (uint32_t y = newH; y < h; ++y) {
                uint32_t idx = y*w + netComponent*w*h;
                std::memset(&m_netInput[idx], 0, w*sizeof(float));
            }
        }
    }

//    {
//        image im;
//        im.c = m_net->c;
//        im.w = m_net->w;
//        im.h = m_net->h;
//        im.data = m_netInput.data();
//        save_image(im, "/tmp/predictions1");
//    }

    network_predict(m_net, m_netInput.data());

    int nboxes = 0;
    float hier = 0.5f;
    int *map = nullptr;
    int relative = 1;
    detection *dets = get_network_boxes(m_net, static_cast<int>(m_inImage.w), static_cast<int>(m_inImage.h), m_netThreshold, hier, map, relative, &nboxes);

    const layer& lastLayer = m_net->layers[m_net->n-1];

    for (int i = 0; i < nboxes; ++i) {
        for(auto expectedLabel : m_expectedLabelColors){
            uint32_t objClass = expectedLabel.first;
            assert(static_cast<int>(objClass) < lastLayer.classes);
            if (dets[i].prob[objClass] > m_netThreshold) {
               m_lastDetections.push_back({objClass,
                                           m_labels[objClass],
                                           dets[i].prob[objClass],
                                           {dets[i].bbox.x, dets[i].bbox.y, dets[i].bbox.w, dets[i].bbox.h}
                                          });
            }
        }
    }

    free_detections(dets, nboxes);

    return !m_lastDetections.empty();
}


const Detector::Image& Detector::getNetOutImg()
{
    uint32_t w = static_cast<uint32_t>(m_net->w);
    uint32_t h = static_cast<uint32_t>(m_net->h);
    uint32_t c = static_cast<uint32_t>(m_net->c);

    if (!m_outNetImageHasLabels) {
        for (uint32_t k = 0; k < c; ++k) {
            for (uint32_t y = 0; y < h; ++y) {
                for (uint32_t x = 0; x < w; ++x) {
                    // convert data from "RRRRRRRRRGGGGGGGGGBBBBBBBBB to "RGBRGBRGBRGBRGBRGBRGBRGBRGB"
                    m_outNetImage.data[(y*w+x)*c+k] = static_cast<uint8_t>(m_netInput[y*w+x + w*h*k] * 255.f);
                }
            }
        }

        drawResults(m_outNetImage.data, m_outNetImage.w, m_outNetImage.h, m_outNetImage.c, m_netScaledImageW, m_netScaledImageH);

        m_outNetImageHasLabels = true;
    }

    return m_outNetImage;
}


const Detector::Image& Detector::getLabeledInImg()
{
    if (!m_inImageHasLabels) {
        drawResults(m_inImage.data, m_inImage.w, m_inImage.h, m_inImage.c);
        m_inImageHasLabels = true;
    }
    return m_inImage;
}

void Detector::drawResults(std::vector<uint8_t>& data, uint32_t w, uint32_t h, uint32_t c)
{
    drawResults(data, w, h, c, w, h);
}

void Detector::drawResults(std::vector<uint8_t>& data, uint32_t w, uint32_t h, uint32_t c, uint32_t validAreaW, uint32_t validAreaH)
{
    assert(data.size() == w*h*c);
    const uint32_t BT = 3; // BT - BORDER THICKNESS
    const int LOWEST_BORDER_CENTER = BT/2; //just to not cast
    const int HIGHEST_H_BORDER_CENTER = static_cast<int>(h - 1 - BT/2);
    const int HIGHEST_W_BORDER_CENTER = static_cast<int>(w - 1 - BT/2);

    for (const DetectionResult& dr : m_lastDetections) {
        uint32_t labelColor = m_expectedLabelColors[static_cast<uint32_t>(dr.classId)];
        std::array<uint8_t, 4> BC; //BC - BORDER_COLOR
        BC[0] = static_cast<uint8_t>(labelColor);
        BC[1] = static_cast<uint8_t>(labelColor >> 8);
        BC[2] = static_cast<uint8_t>(labelColor >> 16);
        BC[3] = static_cast<uint8_t>(labelColor >> 24);

        //first cast to int because potential negative value
        uint32_t left   = static_cast<uint32_t>(std::clamp(static_cast<int>((dr.box.x - dr.box.w / 2.0f) * validAreaW), LOWEST_BORDER_CENTER, HIGHEST_W_BORDER_CENTER));
        uint32_t right  = static_cast<uint32_t>(std::clamp(static_cast<int>((dr.box.x + dr.box.w / 2.0f) * validAreaW), LOWEST_BORDER_CENTER, HIGHEST_W_BORDER_CENTER));
        uint32_t top    = static_cast<uint32_t>(std::clamp(static_cast<int>((dr.box.y - dr.box.h / 2.0f) * validAreaH), LOWEST_BORDER_CENTER, HIGHEST_H_BORDER_CENTER));
        uint32_t bottom = static_cast<uint32_t>(std::clamp(static_cast<int>((dr.box.y + dr.box.h / 2.0f) * validAreaH), LOWEST_BORDER_CENTER, HIGHEST_H_BORDER_CENTER));

        for (uint32_t k = 0; k < c; ++k) {
            uint8_t kVal = BC[k % BC.size()];
            //top
            for (uint32_t y = top - BT/2; y < top + BT/2; ++y) {
                for (uint32_t x = left; x < right; ++x) {
                    data[(y*w+x)*c+k] = kVal;
                }
            }

            //bottom
            for (uint32_t y = bottom - BT/2; y < bottom + BT/2; ++y) {
                for (uint32_t x = left; x < right; ++x) {
                    data[(y*w+x)*c+k] = kVal;
                }
            }

            //left
            for (uint32_t x = left - BT/2; x < left + BT/2; ++x) {
                for (uint32_t y = top; y < bottom; ++y) {
                    data[(y*w+x)*c+k] = kVal;
                }
            }

            //right
            for (uint32_t x = right - BT/2; x < right + BT/2; ++x) {
                for (uint32_t y = top; y < bottom; ++y) {
                    data[(y*w+x)*c+k] = kVal;
                }
            }

            drawString(data, w, h, c, left + BT/2 + 1, top + BT/2 + 1, dr.label, 0.5f, labelColor, true);
        }
    }
}

void Detector::readLabels(const std::string& labelsFilePath, const std::string& expectedLabelsFilePath)
{
    std::set<std::string> expectedLabels;
    if (!expectedLabelsFilePath.empty() && labelsFilePath != expectedLabelsFilePath) {
        std::ifstream expectedLabelsFile(expectedLabelsFilePath);
        if (!expectedLabelsFile.is_open()) {
            std::cerr << "Can't open label file: " << labelsFilePath << "\n";
        }
        else {
            while(expectedLabelsFile.good()) {
                std::string line;
                std::getline(expectedLabelsFile, line);
                if (!line.empty())
                    expectedLabels.insert(line);
            }
        }
    }
    bool addAllLabels = expectedLabels.empty();
    std::set<uint32_t> expectedLabelIds;

    std::ifstream labelsFile(labelsFilePath);
    if (!labelsFile.is_open()) {
        std::cerr << "Can't open label file: " << labelsFilePath << "\n";
    }
    else {
        while(labelsFile.good()) {
            std::string line;
            std::getline(labelsFile, line);
            if (!line.empty()) {
                m_labels.push_back(line); // add label

                //try to find it is expected labels
                if (!addAllLabels) {
                    auto expectedIt = expectedLabels.find(line);
                    if (expectedIt != expectedLabels.end()) {
                        expectedLabelIds.insert(static_cast<uint32_t>(m_labels.size() - 1));
                        expectedLabels.erase(expectedIt); //remove if found - then we report
                    }
                }
                else {
                    expectedLabelIds.insert(static_cast<uint32_t>(m_labels.size() - 1));
                }
            }
        }
    }

    for (auto expectedLabel : expectedLabels) {
        std::cout << "[WARNING] Expected label: " << expectedLabel << ", not found in labels.\n";
    }

    if (!expectedLabelIds.empty()) {
        std::vector<uint32_t> labelColors = ColorGenerator::generateColorsRGB(static_cast<uint32_t>(expectedLabelIds.size()));
        assert(labelColors.size() == expectedLabelIds.size());
        m_expectedLabelColors.clear();

        auto expectedLabelIt = expectedLabelIds.begin();
        for (uint32_t i = 0; i < labelColors.size(); ++i, ++expectedLabelIt) {
            uint32_t labelColor = labelColors[i];
            m_expectedLabelColors.insert(std::make_pair(*expectedLabelIt, labelColor));
        }

        for(auto labelPair : m_expectedLabelColors) {
            printf("%20s - 0x%x\n", m_labels[labelPair.first].c_str(), labelPair.second);
        }

        generateLabelsImg();
    }
}

void Detector::generateLabelsImg() const
{
    uint32_t labelW = 600;
    uint32_t colorX = 40;
    uint32_t textX = 45;

    uint32_t letterH = getLetterData('W').h;
    uint32_t letterMargin = static_cast<uint32_t>(letterH*0.25);
    letterMargin = std::max(letterMargin, 5u);
    uint32_t labelH = letterMargin*2 + letterH;
    uint32_t textY = letterMargin;

    Image labelsImg;
    labelsImg.data.resize(m_expectedLabelColors.size()*labelH*labelW*3, 0);
    labelsImg.c = 3;
    labelsImg.w = labelW;
    labelsImg.h = static_cast<uint32_t>(labelH*m_expectedLabelColors.size());
    uint32_t i = 0;
    for(auto labelPair : m_expectedLabelColors) {
        uint32_t labelId = labelPair.first;
        uint32_t labelColor = labelPair.second;

        const std::string& labelText = m_labels[labelId];

        uint8_t rgb[3];
        rgb[0]  = static_cast<uint8_t>(labelColor);
        rgb[1]  = static_cast<uint8_t>(labelColor >> 8);
        rgb[2]  = static_cast<uint8_t>(labelColor >> 16);

        for (uint32_t y = 0; y < labelH; ++y) {
            for (uint32_t x = 0; x < colorX; ++x) {
                for (uint32_t c = 0; c < 3; ++c)
                    labelsImg.data[i*labelH*labelW*3 + y*labelW*3 + x*3 + c] = rgb[c];
            }
        }

        drawString(labelsImg.data, labelsImg.w, labelsImg.h, labelsImg.c, textX, labelH*i + textY, labelText, 1.0f, labelColor, true);

        ++i;
    }
    writePngFile("/tmp/lables.png", labelsImg.w, labelsImg.h, labelsImg.data.data());
}

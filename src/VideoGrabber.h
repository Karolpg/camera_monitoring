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
#include <cstdint>
#include "FrameController.h"
#include "Config.h"

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;

struct ComponentType
{
    uint32_t componentCount;
    std::string componentStr;
};

class VideoGrabber {
  public:
    ///
    /// \brief VideoGrabber - allow to connect to camera
    ///                       it takes frame of video and provide it to FrameController
    /// \param cfg - config should contains key: "cameraUrl"
    ///
    VideoGrabber(const Config& cfg);
    ~VideoGrabber();

    int onNewVideoSample(GstElement *sink);

    void hangOnPlay();

    FrameController& getFrameController() { return m_frameController; }
  private:

    std::string m_uri;
    std::string m_pipelineCmd;
    static const std::string s_defaultPipelineCmd;
    GstElement* m_pipeline = nullptr;
    GstElement* m_appSink = nullptr;
    GstBus *m_bus = nullptr;

    ComponentType m_componentOut = {3, "RGB"};

    FrameController m_frameController;
};

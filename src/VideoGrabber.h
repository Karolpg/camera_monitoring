#pragma once

#include <string>
#include <cstdint>
#include "FrameControler.h"
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
    GstElement* m_pipeline = nullptr;
    GstElement* m_appSink = nullptr;
    GstBus *m_bus = nullptr;

    ComponentType m_componentOut = {3, "RGB"};

    FrameController m_frameController;
};

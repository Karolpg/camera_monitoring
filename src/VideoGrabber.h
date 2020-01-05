#pragma once

#include <string>
#include <cstdint>
#include "FrameControler.h"

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;

struct ComponentType
{
    uint32_t componentCount;
    std::string componentStr;
};

class VideoGrabber {
  public:
    VideoGrabber(const std::string& videoUri);
    ~VideoGrabber();

    int onNewVideoSample(GstElement *sink);

    void hangOnPlay();

    FrameControler& getFrameControler() { return m_frameControler; }
  private:

    std::string m_uri;
    GstElement* m_pipeline = nullptr;
    GstElement* m_appSink = nullptr;
    GstBus *m_bus = nullptr;

    ComponentType m_componentOut = {3, "RGB"};

    FrameControler m_frameControler;
};

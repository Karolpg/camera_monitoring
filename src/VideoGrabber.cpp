#include "VideoGrabber.h"
#include <gst/gst.h>
#include <iostream>
#include <assert.h>
#include <string>
#include <array>
#include <stdio.h>

const std::string VideoGrabber::s_defaultPipelineCmd = "uridecodebin uri=%s ! videoconvert ! appsink name=mysink";

static GstFlowReturn onNewVideoSample(GstElement *sink, void *ctx)
{
    assert(ctx);
    return GstFlowReturn(reinterpret_cast<VideoGrabber*>(ctx)->onNewVideoSample(sink));
}


VideoGrabber::VideoGrabber(const Config &cfg)
    : m_uri(cfg.getValue("cameraUrl"))
    , m_pipelineCmd(cfg.getValue("gstreamerCmd", s_defaultPipelineCmd))
    , m_frameController(cfg)
{
    // Initialize GStreamer
    gst_init (nullptr, nullptr);

    // Build the pipeline
    if (m_pipelineCmd == s_defaultPipelineCmd) {
        std::array<char, 1024> buffer;
        snprintf(buffer.data(), buffer.size(), s_defaultPipelineCmd.data(), m_uri.data());
        m_pipelineCmd = buffer.data();
    }
    m_pipeline = gst_parse_launch(m_pipelineCmd.c_str(), nullptr);
    if (!m_pipeline) {
        std::cerr << "VideoGrabber: can't create pipeline with string: " << m_pipelineCmd << "\n";
        return;
    }

    // Get own elements
    m_appSink = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysink");
    if (!m_appSink) {
        std::cerr << "VideoGrabber: can't find appsink by name 'mysink' in pipeline .\n";
        return;
    }

    // Configure appsink
    GstCaps *appVideoCaps = gst_caps_new_simple("video/x-raw",
                                                "format", G_TYPE_STRING, m_componentOut.componentStr.c_str(),
                                                nullptr);
    g_object_set(m_appSink, "emit-signals", TRUE, "caps", appVideoCaps, nullptr);
    void *newSampleCtx = this;
    g_signal_connect(m_appSink, "new-sample", G_CALLBACK(::onNewVideoSample), newSampleCtx);
    gst_caps_unref(appVideoCaps);

    m_bus = gst_element_get_bus(m_pipeline);
}

VideoGrabber::~VideoGrabber()
{
    if (m_bus) gst_object_unref(m_bus);
    if (m_appSink) gst_object_unref(m_appSink);
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
    }
}

void VideoGrabber::hangOnPlay()
{
    // Start playing
    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);

    // Wait until error or EOS
    GstMessage *msg = gst_bus_timed_pop_filtered(m_bus,
                                                 GST_CLOCK_TIME_NONE,
                                                 static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg) {
        std::cout << "Msg type: " << gst_message_type_get_name(msg->type)
                  << " src: " << static_cast<void*>(msg->src)
                  << " timestamp: " << msg->timestamp
                  << " seq: " << msg->seqnum
                  << "\n";

        if (msg->type == GST_MESSAGE_ERROR) {
            GError* eData;
            gchar* dData;
            gst_message_parse_error(msg, &eData, &dData);
            std::cerr << "Debug msg: " << dData
                      << "\nError code: " << eData->code
                      << "\nError message: " << eData->message
                      << "\n";
            g_error_free(eData);
            g_free(dData);
        }

        gst_message_unref (msg);
    }
}

int VideoGrabber::onNewVideoSample(GstElement *sink)
{
    GstSample *sample;
    // Retrieve the buffer
    g_signal_emit_by_name(sink, "pull-sample", &sample);

    if (sample) {
        uint32_t width = 0;
        uint32_t height = 0;
        double fps = 0.0;

        GstCaps *sampleCaps = gst_sample_get_caps(sample);
        if (sampleCaps) {
            for (unsigned int i = 0; i < gst_caps_get_size(sampleCaps); i++) {
                GstStructure *structure = gst_caps_get_structure(sampleCaps, i);

                static bool displayStructure = true;
                if (displayStructure) {
                    gchar* allFieldsAndTypes = gst_structure_to_string(structure);
                    printf("=====\n%s\n=====\n", allFieldsAndTypes);
                    g_free(allFieldsAndTypes);
                    displayStructure = false;
                }

                int w = 0;
                int h = 0;
                int fps_n = 0;
                int fps_d = 0;
                gst_structure_get_int(structure, "width", &w);
                gst_structure_get_int(structure, "height", &h);
                gst_structure_get_fraction(structure, "framerate", &fps_n, &fps_d);
                width = w > 0 ? static_cast<uint32_t>(w) : 0;
                height = h > 0 ? static_cast<uint32_t>(h) : 0;
                fps = (fps_d == 0 ? 0.0 : static_cast<double>(fps_n) / fps_d);
                const gchar* dataFormat = gst_structure_get_string(structure, "format");
                if (m_componentOut.componentStr != dataFormat) {
                    std::cerr << "Expected format is: " << m_componentOut.componentStr << ", while given format is: " << dataFormat << "\n";
                }
            }
        }

        GstBuffer *sampleBuffer = gst_sample_get_buffer(sample);
        if (sampleBuffer) {
            if(width * height > 0) {

                if (width != m_frameController.getWidth()
                        || height != m_frameController.getHeight()
                        || m_componentOut.componentCount != m_frameController.getComponents()) {
                    printf("Dim %dx%d Format:%s Fps:%lf\n", width, height, m_componentOut.componentStr.c_str(), fps);
                    double timeLength = 1.5; // [s]
                    m_frameController.setBufferParams(timeLength, fps, width, height, m_componentOut.componentCount);
                }

                GstMapInfo map;
                if (gst_buffer_map(sampleBuffer, &map, GST_MAP_READ)) {

                    m_frameController.addFrame(static_cast<const uint8_t*>(map.data));

                    gst_buffer_unmap(sampleBuffer, &map);
                }
            }
        }

        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }
    return GST_FLOW_ERROR;
}

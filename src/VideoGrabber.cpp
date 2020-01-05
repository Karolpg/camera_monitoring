#include "VideoGrabber.h"
#include <gst/gst.h>
#include <iostream>
#include <assert.h>
#include <string>

static GstFlowReturn onNewVideoSample(GstElement *sink, void *ctx)
{
    assert(ctx);
    return GstFlowReturn(reinterpret_cast<VideoGrabber*>(ctx)->onNewVideoSample(sink));
}


VideoGrabber::VideoGrabber(const std::string& videoUri) : m_uri(videoUri)
{
    // Initialize GStreamer
    gst_init (nullptr, nullptr);

    // Build the pipeline
    std::string pipelineStr = std::string("uridecodebin uri=") + m_uri + " ! videoconvert ! appsink name=mysink";
    m_pipeline = gst_parse_launch(pipelineStr.c_str(), nullptr);
    if (!m_pipeline) {
        std::cerr << "VideoGrabber: can't create pipeline with string: " << pipelineStr << "\n";
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
    GstMessage *msg = gst_bus_timed_pop_filtered(m_bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    if (msg != nullptr)
        gst_message_unref(msg);
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

                if (width != m_frameControler.getWidth()
                        || height != m_frameControler.getHeight()
                        || m_componentOut.componentCount != m_frameControler.getComponents()) {
                    printf("Dim %dx%d Format:%s Fps:%lf\n", width, height, m_componentOut.componentStr.c_str(), fps);
                    double timeLength = 3.0; // [s]
                    m_frameControler.setBufferParams(timeLength, fps, width, height, m_componentOut.componentCount);
                }

                GstMapInfo map;
                if (gst_buffer_map(sampleBuffer, &map, GST_MAP_READ)) {

                    m_frameControler.addFrame(static_cast<const uint8_t*>(map.data));

                    gst_buffer_unmap(sampleBuffer, &map);
                }
            }
        }

        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }
    return GST_FLOW_ERROR;
}

#include "VideoRecorder.h"

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/video/video.h>
#include <gst/base/gstbasesink.h>

#include <iostream>
#include <assert.h>

#include "PngTools.h"

VideoRecorder::VideoRecorder(const std::string &outFilePath, const RecordingDataType &recDataType)
    : m_outFilePath(outFilePath)
    , m_recDataType(recDataType)
{
    if (!createPipeline()) {
        return;
    }

    m_videoAppSource = gst_bin_get_by_name(GST_BIN(m_pipeline), "myAppSrc");
    if (!m_videoAppSource) {
        std::cerr << "Can't find appsrc element in pipeline!\n";
        return;
    }

    m_outFileObj = gst_bin_get_by_name(GST_BIN(m_pipeline), "outFileObj");
    if (!m_outFileObj) {
        std::cerr << "Can't find filesink element in pipeline!\n";
        return;
    }

    fillCapabilities();

    g_object_set(m_videoAppSource, "format", GST_FORMAT_TIME, NULL);
    g_signal_connect(m_videoAppSource, "need-data", G_CALLBACK(videoAppSrcNeedData), this);
    g_signal_connect(m_videoAppSource, "enough-data", G_CALLBACK(videoAppSrcEnoughData), this);

    m_bus = gst_element_get_bus(m_pipeline);
    if (!m_bus) {
        std::cerr << "Can't get bus from pipeline!\n";
        return;
    }

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);

    m_pipelineThread = std::thread(pipelineLoop, std::ref(*this));
    m_videoFeedingThread =  std::thread(videoFeedingLoop, std::ref(*this));

    m_gstComponentsOk = true;
}

VideoRecorder::~VideoRecorder()
{
    finishRecording();
    m_pipelineThread.join();
    if (m_bus) gst_object_unref(m_bus);
    if (m_videoAppSource) gst_object_unref(m_videoAppSource);
    if (m_outFileObj) gst_object_unref(m_outFileObj);
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
    }

    m_videoFeed = false;
    m_videoCv.notify_all();
    m_waitingRecordingFinishCv.notify_all();
    m_videoFeedingThread.join();
}

bool VideoRecorder::createPipeline()
{
    std::string pipelineDefinition = std::string("appsrc name=myAppSrc ! videoconvert ! queue ! x264enc ! filesink name=outFileObj location=") + m_outFilePath;
    GError* error = nullptr;
    m_pipeline = gst_parse_launch(pipelineDefinition.c_str(), &error);
    if (error) {
        std::cerr << "Error during creating pipeline:" << pipelineDefinition
                  << "\n    Error code: " << error->code
                  << "\n    Error message: " << error->message
                  << "\n";
        g_error_free(error);
        return false;
    }
    else if (!m_pipeline) {
        std::cerr << "Error during creating pipeline:" << pipelineDefinition
                  << "\n    Didn't get any error!\n";
        return false;
    }

    return true;
}

void VideoRecorder::fillCapabilities()
{
    GstCaps *audioCaps = nullptr;
    if (m_recDataType.useAudio) {
        GstAudioInfo audioInfo;
        gst_audio_info_set_format(&audioInfo,
                                  m_recDataType.audioFormat,
                                  static_cast<gint>(m_recDataType.audioSampleRate),
                                  static_cast<gint>(m_recDataType.audioChannels),
                                  nullptr);
        m_audioBytePerSample = static_cast<uint32_t>(GST_AUDIO_INFO_BPS(&audioInfo));
        audioCaps = gst_audio_info_to_caps (&audioInfo);
    }

    GstCaps *videoCaps = nullptr;
    if (m_recDataType.useVideo) {
        GstVideoInfo videoInfo;
        gst_video_info_init(&videoInfo);
        gst_video_info_set_format(&videoInfo,
                                  m_recDataType.videoFormat,
                                  m_recDataType.videoW,
                                  m_recDataType.videoH);
        videoInfo.fps_n = static_cast<gint>(m_recDataType.videoFpsN);
        videoInfo.fps_d = static_cast<gint>(m_recDataType.videoFpsD);
        videoCaps = gst_video_info_to_caps(&videoInfo);
    }

    if (m_recDataType.useAudio && m_recDataType.useVideo) {
        GstCaps *audioVideo = gst_caps_merge(audioCaps, videoCaps);
        g_object_set(m_videoAppSource, "caps", audioVideo, NULL);
        gst_caps_unref(audioVideo);
        gst_caps_unref(audioCaps);
        gst_caps_unref(videoCaps);
    }
    else if (m_recDataType.useAudio) {
        g_object_set(m_videoAppSource, "caps", audioCaps, NULL);
        gst_caps_unref(audioCaps);
    }
    else if (m_recDataType.useVideo) {
        g_object_set(m_videoAppSource, "caps", videoCaps, NULL);
        gst_caps_unref(videoCaps);
    }
}

void VideoRecorder::pipelineLoop(VideoRecorder& vr)
{
    GstMessage *msg = gst_bus_timed_pop_filtered(vr.m_bus,
                                                 GST_CLOCK_TIME_NONE,
                                                 static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    auto errorHandling = [&vr](GstMessage *msg) {
        GError* eData;
        gchar* dData;
        gst_message_parse_error(msg, &eData, &dData);
        std::cerr << "Debug msg: " << dData
                  << "\nError code: " << eData->code
                  << "\nError message: " << eData->message
                  << "\n";
        vr.m_gstComponentsOk = false;
        g_error_free(eData);
        g_free(dData);
    };

    if (msg) {
        std::cout << "Msg type: " << gst_message_type_get_name(msg->type)
                  << " src: " << static_cast<void*>(msg->src)
                  << " timestamp: " << msg->timestamp
                  << " seq: " << msg->seqnum
                  << "\n";

        bool eos = false;
        if (msg->type == GST_MESSAGE_ERROR) {
            errorHandling(msg);
        }
        else if(msg->type == GST_MESSAGE_EOS) {
            eos = true;
        }

        gst_message_unref (msg);

        //if (eos) {
        //    GstBaseSink* outFileSink = reinterpret_cast<GstBaseSink*>(vr.m_outFileObj);
        //    while (!outFileSink->eos) {
        //        GstMessage *msg = gst_bus_timed_pop_filtered(vr.m_bus,
        //                                                     5 * 1000,
        //                                                     static_cast<GstMessageType>(GST_MESSAGE_ERROR));
        //        if (msg && msg->type == GST_MESSAGE_ERROR) {
        //            errorHandling(msg);
        //            break;
        //        }
        //    }
        //}

        vr.m_recordingFinished = true;
        vr.m_waitingRecordingFinishCv.notify_one();
    }
}

void VideoRecorder::videoFeedingLoop(VideoRecorder &vr)
{
    while (vr.m_videoFeed) {

        GstBuffer *videoBuffer = nullptr;
        {
            std::unique_lock<std::mutex> ul(vr.m_videoDataMutex);
            vr.m_videoCv.wait(ul, [&vr]() {
                return (!vr.m_videoDataQueue.empty() && vr.m_videoNeedData) || !vr.m_videoFeed || vr.m_sendFinishRecording;
            });

            if (vr.m_sendFinishRecording && (!vr.m_videoFeed || vr.m_videoDataQueue.empty())) {
                vr.m_sendFinishRecording = false;
                GstEvent *eosEvent = gst_event_new_eos();
                auto result = gst_element_send_event(vr.m_pipeline, eosEvent);
                if (!result) {
                    std::cerr << "Can't send end of stream event\n";
                    gst_event_unref(eosEvent);
                }
                vr.m_videoFeed = false; // there is no need to do more here, we can end this thread
                return;
            }

            if (!vr.m_videoFeed) {
                return;
            }
            videoBuffer = vr.m_videoDataQueue.front();
            vr.m_videoDataQueue.pop();
        }

        GstFlowReturn ret = GST_FLOW_OK;
        g_signal_emit_by_name(vr.m_videoAppSource, "push-buffer", videoBuffer, &ret);

        if (ret != GST_FLOW_OK) {
            std::cerr << "Error while emiting video push-buffer:" << ret << "\n";
        }

        gst_buffer_unref(videoBuffer);
    }
}

void VideoRecorder::videoAppSrcNeedData(GstElement *, guint, VideoRecorder *vr)
{
    vr->m_videoNeedData = true;

    if(vr->m_videoDataQueue.empty() && vr->m_finishRecordingAlreadyScheduled) {
        vr->m_sendFinishRecording = true;
    }
    vr->m_videoCv.notify_one();
}

void VideoRecorder::videoAppSrcEnoughData(GstElement *, VideoRecorder *vr)
{
    vr->m_videoNeedData = false;
}

bool VideoRecorder::addFrame(const StreamData &videoData)
{
    if (!m_gstComponentsOk) {
        std::cerr << "Some errors occured in gstreamer while trying to finish recording\n";
        return false;
    }

    if (!m_recDataType.useVideo) {
        std::cerr << "Video data is not expected!\n";
        return false;
    }

    if (m_finishRecordingAlreadyScheduled) {
        std::cerr << "Finish has been sent!\n";
        return false;
    }

    GstBuffer *videoBuffer = gst_buffer_new_and_alloc(videoData.size());

    GST_BUFFER_TIMESTAMP(videoBuffer) = gst_util_uint64_scale(m_videoSampleCnt, GST_SECOND * m_recDataType.videoFpsD, m_recDataType.videoFpsN);
    GST_BUFFER_DURATION(videoBuffer) = gst_util_uint64_scale(1, GST_SECOND * m_recDataType.videoFpsD, m_recDataType.videoFpsN);

    GstMapInfo map;
    gst_buffer_map(videoBuffer, &map, GST_MAP_WRITE);

    StreamData::value_type *raw = reinterpret_cast<StreamData::value_type*>(map.data);
    std::copy(videoData.begin(), videoData.end(), raw);

    gst_buffer_unmap(videoBuffer, &map);

    ++m_videoSampleCnt;

    std::unique_lock<std::mutex> ul(m_videoDataMutex);

    m_videoDataQueue.push(videoBuffer); // TODO maybe add some limitation
    m_videoCv.notify_one();

    return true;
}

bool VideoRecorder::addAudioSamples(const StreamData& audioData)
{
    if (!m_gstComponentsOk) {
        std::cerr << "Some errors occured in gstreamer while trying to add audio samples\n";
        return false;
    }

    if (!m_recDataType.useAudio) {
        std::cerr << "Audio data is not expected!\n";
        return false;
    }

    if (m_finishRecordingAlreadyScheduled) {
        std::cerr << "Finish has been sent!\n";
        return false;
    }

    assert(!m_recDataType.useAudio && "Not finished! :(");

    GstBuffer *audioBuffer = gst_buffer_new_and_alloc(audioData.size());

    uint32_t samples = static_cast<uint32_t>(audioData.size() / m_audioBytePerSample);

    GST_BUFFER_TIMESTAMP(audioBuffer) = gst_util_uint64_scale(m_audioSampleCnt, GST_SECOND, m_recDataType.audioSampleRate);
    GST_BUFFER_DURATION(audioBuffer) = gst_util_uint64_scale(samples, GST_SECOND, m_recDataType.audioSampleRate);

    GstMapInfo map;
    gst_buffer_map(audioBuffer, &map, GST_MAP_WRITE);

    StreamData::value_type *raw = reinterpret_cast<StreamData::value_type*>(map.data);
    std::copy(audioData.begin(), audioData.end(), raw);

    gst_buffer_unmap(audioBuffer, &map);

    m_audioSampleCnt += samples;

    std::lock_guard lg(m_audioDataMutex);
    m_audioDataQueue.push(audioBuffer); // TODO maybe add some limitation

    return true;
}

bool VideoRecorder::finishRecording()
{
    if (!m_gstComponentsOk) {
        std::cerr << "Some errors occured in gstreamer while trying to finish recording\n";
        return false;
    }

    if (m_finishRecordingAlreadyScheduled) {
        m_videoCv.notify_one();
        return true;
    }

    m_finishRecordingAlreadyScheduled = true;
    m_videoCv.notify_one();
    return true;
}

void VideoRecorder::waitForFinish()
{
    if (!m_gstComponentsOk) {
        std::cerr << "Some errors occured in gstreamer while trying to wait for finish recording\n";
        return;
    }

    if (!m_finishRecordingAlreadyScheduled) {
        finishRecording();
    }

    std::unique_lock<std::mutex> ul(m_waitingRecordingMutex);
    m_waitingRecordingFinishCv.wait(ul, [this]() { return m_recordingFinished; });
}

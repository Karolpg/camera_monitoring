#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>
#include <queue>

#include <gst/audio/audio-format.h>
#include <gst/video/video-format.h>

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
typedef struct _GstBuffer GstBuffer;

struct RecordingDataType
{
    bool useAudio;
    GstAudioFormat audioFormat;
    uint32_t audioSampleRate;
    uint32_t audioChannels;

    bool useVideo;
    GstVideoFormat videoFormat;
    uint32_t videoW;
    uint32_t videoH;
    uint32_t videoFpsN; // nominator
    uint32_t videoFpsD; // denominator
};

class VideoRecorder
{
public:
    typedef std::vector<uint8_t> StreamData;

    VideoRecorder(const std::string& outFilePath, const RecordingDataType& recDataType);
    ~VideoRecorder();

    bool hasError() const { return !m_gstComponentsOk; }

    /// Data has to be provided in declared format. Size is not checked.
    bool addFrame(const StreamData& videoData);

    /// Data has to be provided in declared format. Size is not checked.
    /// Currently audio is not fully implemented by me!!! :(
    bool addAudioSamples(const StreamData& audioData);

    bool finishRecording();
private:
    bool createPipeline();
    void fillCapabilities();

    static void pipelineLoop(VideoRecorder& vr);
    static void videoFeedingLoop(VideoRecorder& vr);
    static void videoAppSrcNeedData(GstElement *, guint, VideoRecorder* vr);
    static void videoAppSrcEnoughData(GstElement *, VideoRecorder* vr);

    std::thread m_pipelineThread;
    std::thread m_videoFeedingThread;
    std::string m_outFilePath;
    RecordingDataType m_recDataType;

    GstElement *m_pipeline = nullptr;
    GstElement *m_videoAppSource = nullptr;
    GstBus* m_bus = nullptr;
    volatile bool m_gstComponentsOk = false;
    volatile bool m_videoFeed = true;
    volatile bool m_videoNeedData = false;
    std::mutex m_audioDataMutex;
    std::mutex m_videoDataMutex;
    std::condition_variable m_videoCv;
    std::queue<GstBuffer*> m_audioDataQueue;
    std::queue<GstBuffer*> m_videoDataQueue;
    uint64_t m_audioSampleCnt = 0;
    uint64_t m_videoSampleCnt = 0;
    uint32_t m_audioBytePerSample = 0; // TODO check how bits are expected in buffer, are they tightly packed for 20, 18 ... bits per sameple format
    bool m_finishRecordingAlreadySent = false;
};

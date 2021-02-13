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

    const std::string& recordingFilePath() const { return m_outFilePath; }

    bool hasError() const { return !m_gstComponentsOk; }

    /// Data has to be provided in declared format. Size is not checked.
    bool addFrame(const StreamData& videoData);

    /// Data has to be provided in declared format. Size is not checked.
    /// Currently audio is not fully implemented by me!!! :(
    bool addAudioSamples(const StreamData& audioData);

    bool finishRecording();
    void waitForFinish();
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
    GstElement *m_outFileObj = nullptr;
    GstBus* m_bus = nullptr;
    volatile bool m_gstComponentsOk = false;
    volatile bool m_videoFeed = true;
    volatile bool m_videoNeedData = false;
    volatile bool m_recordingFinished = false;
    std::mutex m_audioDataMutex;
    std::mutex m_videoDataMutex;
    std::mutex m_waitingRecordingMutex;
    std::condition_variable m_videoCv;
    std::condition_variable m_waitingRecordingFinishCv;
    std::queue<GstBuffer*> m_audioDataQueue;
    std::queue<GstBuffer*> m_videoDataQueue;
    uint64_t m_audioSampleCnt = 0;
    uint64_t m_videoSampleCnt = 0;
    uint32_t m_audioBytePerSample = 0; // TODO check how bits are expected in buffer, are they tightly packed for 20, 18 ... bits per sameple format
    bool m_finishRecordingAlreadyScheduled = false;
    bool m_sendFinishRecording = false;
};

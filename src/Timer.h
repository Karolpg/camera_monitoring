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
#include <chrono>
#include <functional>
#include <condition_variable>

template <typename Func>
class Timer
{
public:
    Timer()
    {
        std::unique_lock<std::mutex> ul(m_waitForTaskMtx);

        m_thread = std::thread([this]()
        {
            while(!m_destroy) {
                {
                    std::unique_lock<std::mutex> ul(m_waitForTaskMtx);
                    m_isWaitingForTask = true;
                    m_waitForTask.notify_one(); //notify stop
                    m_waitForTask.wait(ul, [this] () {return !m_isWaitingForTask; });
                }
                if (m_destroy) {
                    break;
                }

                bool runPeriodic = false;
                do
                {
                    //std::this_thread::sleep_for(m_delay);
                    {
                        std::unique_lock<std::mutex> ul(m_delayMtx);
                        std::cv_status result = m_delayCv.wait_for(ul, m_delay);
                        if (result != std::cv_status::timeout) {
                            break;
                        }
                    }
                    m_fun();
                    {
                        std::lock_guard<std::mutex> lg(m_stopMtx);
                        runPeriodic = m_runPeriodic;
                    }

                }
                while (runPeriodic);
            }
        });

        m_waitForTask.wait(ul, [this] () { return m_isWaitingForTask; });
        // we want to be sure thread has been started
    }

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    ~Timer()
    {
        stop();
        m_destroy = true;
        m_isWaitingForTask = false;
        m_waitForTask.notify_all();
        m_thread.join();
    }

    void runOnce(Func func, std::chrono::microseconds delay)
    {
        stop();
        // here we are stopped == thread safe
        m_fun = func;
        m_isWaitingForTask = false;
        m_runPeriodic = false;
        m_delay = delay;
        m_waitForTask.notify_one();
    }

    void runEvery(Func func, std::chrono::microseconds delay)
    {
        stop();
        // here we are stopped == thread safe
        m_fun = func;
        m_isWaitingForTask = false;
        m_runPeriodic = true;
        m_delay = delay;
        m_waitForTask.notify_one();
    }

    void stop()
    {
        std::unique_lock<std::mutex> ul(m_waitForTaskMtx);
        if (m_isWaitingForTask)
            return; //already stopped
        m_delayCv.notify_all();
        {
            std::lock_guard<std::mutex> lg(m_stopMtx);
            m_runPeriodic = false;
        }

        m_waitForTask.wait(ul, [this]() { return m_isWaitingForTask; });
    }
private:
    volatile bool m_destroy{false};
    volatile bool m_runPeriodic{false};
    volatile bool m_isWaitingForTask{false};
    Func m_fun;

    std::condition_variable m_waitForTask;
    std::mutex m_waitForTaskMtx;
    std::thread m_thread;

    std::chrono::microseconds m_delay;
    std::condition_variable m_delayCv;
    std::mutex m_delayMtx;

    std::mutex m_stopMtx;
};

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
        m_isRunning = true;

        m_thread = std::thread([this]()
        {
            while(!m_destroy) {
                {
                    std::unique_lock<std::mutex> ul(m_waitForTaskMtx);
                    m_isRunning = false;
                    m_waitForTask.notify_one(); //notify stop
                    m_waitForTask.wait(ul, [this] () {return m_isRunning; });
                }
                if (m_destroy) {
                    break;
                }

                bool runPeriodic = false;
                do
                {
                    std::this_thread::sleep_for(m_delay);
                    m_fun();
                    {
                        std::lock_guard<std::mutex> lg(m_stopMtx);
                        runPeriodic = m_runPeriodic;
                    }

                }
                while (runPeriodic);
            }
        });

        m_waitForTask.wait(ul, [this] () {return !m_isRunning; });
        // we want to be sure thread has been started
    }

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    ~Timer()
    {
        stop();
        m_destroy = true;
        m_isRunning = true;
        m_waitForTask.notify_all();
        m_thread.join();
    }

    void runOnce(Func func, std::chrono::microseconds delay)
    {
        stop();
        // here we are stopped == thread safe
        m_fun = func;
        m_isRunning = true;
        m_runPeriodic = false;
        m_delay = delay;
        m_waitForTask.notify_one();
    }

    void runEvery(Func func, std::chrono::microseconds delay)
    {
        stop();
        // here we are stopped == thread safe
        m_fun = func;
        m_isRunning = true;
        m_runPeriodic = true;
        m_delay = delay;
        m_waitForTask.notify_one();
    }

    void stop()
    {
        std::unique_lock<std::mutex> ul(m_waitForTaskMtx);
        if (!m_isRunning)
            return; //already stopped

        {
            std::lock_guard<std::mutex> lg(m_stopMtx);
            m_runPeriodic = false;
        }

        m_waitForTask.wait(ul, [this]() { return !m_isRunning; });
    }
private:
    volatile bool m_destroy{false};
    volatile bool m_runPeriodic{false};
    volatile bool m_isRunning{false};
    Func m_fun;

    std::condition_variable m_waitForTask;
    std::mutex m_waitForTaskMtx;
    std::thread m_thread;
    std::chrono::microseconds m_delay;

    std::mutex m_stopMtx;
};

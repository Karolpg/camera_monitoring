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
#include <map>
#include <list>
#include <memory>
#include <optional>


class WorkerThread;

class Job
{
public:
    explicit Job(std::optional<std::chrono::microseconds> periodic = std::optional<std::chrono::microseconds>());
    virtual ~Job();

protected:
    virtual void doJob() = 0;
    virtual void cancelJob() = 0;

private:
    // below job parameter is provided as shared ptr because we would like to base on some existing shared_ptr state
    // I can't do std::shared_ptr<Job>(this) as it will became new shared_ptr!
    void run(std::shared_ptr<Job> job);
    void cancel();

private:
    std::optional<std::chrono::microseconds> m_periodic;
    WorkerThread *m_executor = nullptr;
    friend class WorkerThread;
    volatile bool m_isRunning = false;
};

class SimpleJob : public Job
{
public:
    SimpleJob(std::function<void()> func, std::optional<std::chrono::microseconds> periodic = std::optional<std::chrono::microseconds>());
protected:
    void doJob() override;
    void cancelJob() override {}
private:
    std::function<void()> m_func;
};

class WorkerThread
{
public:
    explicit WorkerThread();
    ~WorkerThread();

    void addJob(std::chrono::microseconds delay, std::shared_ptr<Job> job);
    void removeJob(std::shared_ptr<Job> job);
    void removeJob(const Job *job);
    void removeAllJobs();

private:
    struct NextTask
    {
        bool noTasksToDo = true;
        std::shared_ptr<Job> job;
        std::chrono::microseconds sleepTime;
    };

    NextTask takeFirstJob();

    std::mutex m_jobsMtx;
    std::map<std::chrono::steady_clock::time_point, std::list<std::shared_ptr<Job>>> m_jobs;

    volatile bool m_destroy{false};

    std::condition_variable m_waitForTask;
    std::mutex m_waitForTaskMtx;
    std::thread m_thread;
};

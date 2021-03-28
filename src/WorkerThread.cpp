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

#include "WorkerThread.h"
#include <iostream>
#include <assert.h>


Job::Job(std::optional<std::chrono::microseconds> periodic)
    : m_periodic(periodic)
{

}

Job::~Job()
{
    cancel();
}


void Job::run(std::shared_ptr<Job> job) {
    assert(job.get() == this);
    m_isRunning = true;
    assert(m_executor);
    doJob();
    if (m_periodic.has_value()) {
        m_executor->addJob(m_periodic.value(), job);
    }
    m_isRunning = false;
}
void Job::cancel() {
    m_periodic = std::optional<std::chrono::microseconds>(); // clear periodicity
    if (m_executor) {
        m_executor->removeJob(this);
    }
    if (m_isRunning) {
        cancelJob();
    }
}

SimpleJob::SimpleJob(std::function<void()> func, std::optional<std::chrono::microseconds> periodic)
    : Job(periodic), m_func(func)
{

}

void SimpleJob::doJob()
{
    assert(m_func);
    m_func();
}

WorkerThread::WorkerThread()
{
    std::unique_lock<std::mutex> ul(m_waitForTaskMtx);

    m_thread = std::thread([this]()
    {
        while(!m_destroy) {
            NextTask taskToDo = takeFirstJob();
            if (taskToDo.noTasksToDo) {
                // there is no task in list
                std::unique_lock<std::mutex> ul(m_waitForTaskMtx);
                m_waitForTask.wait(ul, [this] () { std::lock_guard<std::mutex> lg(m_jobsMtx); return !m_jobs.empty(); });
                continue;
            }
            else  if(!taskToDo.job) {
                // we have to wait for task
                // but there still could be situation when someone adds new task ealier
                std::unique_lock<std::mutex> ul(m_waitForTaskMtx);
                m_waitForTask.wait_for(ul, taskToDo.sleepTime);
                continue;
            }
            if (m_destroy) {
                break;
            }

            assert(taskToDo.job);

            taskToDo.job->run(taskToDo.job);
        }
    });
}

WorkerThread::~WorkerThread()
{
    m_destroy = true;
    m_waitForTask.notify_all();
    m_thread.join();
}


WorkerThread::NextTask WorkerThread::takeFirstJob()
{
    NextTask taskToDo;
    std::lock_guard<std::mutex> lg(m_jobsMtx);
    if (!m_jobs.empty()) {
        taskToDo.noTasksToDo = false;

        auto firstJobsIt = m_jobs.begin();
        auto timestamp = firstJobsIt->first;
        auto& inTimeJobs = firstJobsIt->second;
        assert(!inTimeJobs.empty()); // assume there is no empty list, I have to keep it clean!

        auto job = inTimeJobs.front();
        auto currentTimestamp = std::chrono::steady_clock::now();

        auto tsUs = std::chrono::duration_cast<std::chrono::microseconds>(timestamp.time_since_epoch()).count();
        auto currentTsUs = std::chrono::duration_cast<std::chrono::microseconds>(currentTimestamp.time_since_epoch()).count();
        if (tsUs <= currentTsUs) { // I want [us] scale
            taskToDo.job = job; // we don't have to wait task is ready
            taskToDo.sleepTime = std::chrono::microseconds(0);
            inTimeJobs.erase(inTimeJobs.begin()); // remove job from list
            if (inTimeJobs.empty()) {
                m_jobs.erase(firstJobsIt);
            }
        }
        else {
            taskToDo.sleepTime = std::chrono::duration_cast<std::chrono::microseconds>(timestamp - currentTimestamp);

        }
    }
    return taskToDo;
}


void WorkerThread::addJob(std::chrono::microseconds delay, std::shared_ptr<Job> job)
{
    if (!job) {
        std::cerr << "Adding empty job is prohibited!\n";
        assert(!"Adding empty job is prohibited!");
        return;
    }
    if (job->m_executor != nullptr && job->m_executor != this) {
        std::cerr << "Job already in use!\n";
        assert(!"Job already in use!");
        return;
    }

    std::lock_guard<std::mutex> lg(m_jobsMtx);

    std::chrono::steady_clock::time_point tp = std::chrono::steady_clock::now() + delay;
    job->m_executor = this;
    m_jobs[tp].push_back(job);
    m_waitForTask.notify_one();
}

void WorkerThread::removeJob(std::shared_ptr<Job> job)
{
    removeJob(job.get());
}

void WorkerThread::removeJob(const Job *job)
{
    std::lock_guard<std::mutex> lg(m_jobsMtx);

    for (auto jobsInTimeIt = m_jobs.begin(); jobsInTimeIt != m_jobs.end(); ++jobsInTimeIt) {
        auto& jobs = jobsInTimeIt->second;
        for (auto jobsIt = jobs.begin(); jobsIt != jobs.end(); ++jobsIt) {
            if (jobsIt->get() == job) {
                (*jobsIt)->m_executor = nullptr;
                jobs.erase(jobsIt);
                if (jobs.empty()) {
                    m_jobs.erase(jobsInTimeIt);
                }
                return;
            }
        }
    }
}

void WorkerThread::removeAllJobs()
{
    std::lock_guard<std::mutex> lg(m_jobsMtx);
    m_jobs.clear();
}

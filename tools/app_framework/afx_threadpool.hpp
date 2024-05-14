// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include <vector>
#include <deque>
#include <functional>
#include <cassert>
#include <thread>
#include <condition_variable>

class RpsAfxThreadPool
{
public:
    enum class JobStatus
    {
        Pending,
        Executing,
        Finished,
        Free,
        Unknown,
    };

    class WaitHandle final
    {
        RpsAfxThreadPool*  m_pPool;
        uint32_t           m_jobId;
    public:
        WaitHandle()
            : m_pPool(nullptr)
            , m_jobId(UINT32_MAX)
        {
        }

        WaitHandle(RpsAfxThreadPool* pPool, uint32_t jobId)
            : m_pPool(pPool)
            , m_jobId(jobId)
        {
        }

        ~WaitHandle()
        {
            CleanUp();
        }

        WaitHandle(const WaitHandle&) = delete;
        WaitHandle& operator=(const WaitHandle&) = delete;

        WaitHandle(WaitHandle&& r)
            : m_pPool(nullptr)
        {
            if (&r != this)
            {
                Assign(r.m_pPool, r.m_jobId);
                r.m_pPool = nullptr;
            }
        }

        WaitHandle& operator=(WaitHandle&& r)
        {
            if (&r != this)
            {
                CleanUp();
                Assign(r.m_pPool, r.m_jobId);
                r.m_pPool = nullptr;
            }
            return *this;
        }

        uint32_t GetJobId() const
        {
            return m_jobId;
        }

        const RpsAfxThreadPool* GetPool() const
        {
            return m_pPool;
        }

        uint32_t Detatch()
        {
            m_pPool = nullptr;
            return m_jobId;
        }

        operator bool() const
        {
            return m_pPool != nullptr;
        }

    private:
        void Assign(RpsAfxThreadPool* pPool, uint32_t jobId)
        {
            assert(m_pPool == nullptr);

            m_pPool = pPool;
            m_jobId = jobId;
        }

        void CleanUp()
        {
            if (m_pPool)
            {
                m_pPool->RemoveWaiter(m_jobId);
            }
        }
    };

private:
    struct Job
    {
        JobStatus             status    = JobStatus::Free;
        bool                  hasWaiter = false;
        std::function<void()> func;
    };

public:

    ~RpsAfxThreadPool()
    {
        Destroy();
    }

    bool Init(uint32_t numThreads)
    {
        Destroy();

        m_workerThreads.resize(numThreads);

        for (uint32_t i = 0; i < numThreads; i++)
        {
            m_workerThreads[i] = std::thread([this]() { WorkerThreadProc(); });
        }

        return true;
    }

    void Destroy()
    {
        m_exiting = true;
        m_jobAddedCV.notify_all();

        for (auto& t : m_workerThreads)
        {
            if (t.joinable())
                t.join();
        }
        m_exiting = false;
        m_workerThreads.clear();
    }

    WaitHandle EnqueueJob(std::function<void()> func)
    {
        if (!m_exiting && !m_workerThreads.empty())
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            uint32_t jobSlot = UINT32_MAX;
            if (m_freeJobSlots.empty())
            {
                jobSlot = (uint32_t)m_jobs.size();
                m_jobs.emplace_back();
            }
            else
            {
                jobSlot = m_freeJobSlots.back();
                m_freeJobSlots.pop_back();
            }

            Job& job = m_jobs[jobSlot];

            job.status    = JobStatus::Pending;
            job.hasWaiter = true;
            job.func      = func;

            m_jobQueue.push_back(jobSlot);

            if (m_activeThreads < m_workerThreads.size())
            {
                m_jobAddedCV.notify_one();
            }

            return WaitHandle(this, jobSlot);
        }
        else
        {
            func();
        }

        return WaitHandle();
    }

    void WaitIdle()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_jobCompletedCV.wait(lock, [this]() { return (m_activeThreads == 0) && m_jobQueue.empty(); });
    }

    void WaitForJobs(WaitHandle* waitHandles, uint32_t numWaitHandles)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_jobCompletedCV.wait(lock, [&]() {
            for (uint32_t i = 0; i < numWaitHandles; i++)
            {
                if (waitHandles[i].GetPool() == this)
                {
                    if (m_jobs[waitHandles[i].GetJobId()].status >= JobStatus::Finished)
                    {
                        uint32_t jobId = waitHandles[i].Detatch();
                        RemoveWaiterNoLock(jobId);
                    }
                    else
                    {
                        break;
                    }
                }

                if (i != numWaitHandles - 1)
                {
                    std::swap(waitHandles[i], waitHandles[numWaitHandles - 1]);
                    i--;
                }
                numWaitHandles--;
            }
            return numWaitHandles == 0;
        });
    }

    uint32_t GetNumThreads() const
    {
        return (uint32_t)m_workerThreads.size();
    }

private:

    void RemoveWaiter(uint32_t jobId)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        RemoveWaiterNoLock(jobId);
    }

    void RemoveWaiterNoLock(uint32_t jobId)
    {
        assert(m_jobs[jobId].hasWaiter);
        m_jobs[jobId].hasWaiter = false;

        if (m_jobs[jobId].status == JobStatus::Finished)
        {
            m_jobs[jobId].status = JobStatus::Free;
            m_freeJobSlots.push_back(jobId);
        }
    }

    void WorkerThreadProc()
    {
        while (!m_exiting)
        {
            uint32_t jobIdx = UINT32_MAX;

            std::function<void()> func;

            {
                std::unique_lock<std::mutex> lock(m_mutex);

                m_jobAddedCV.wait(lock, [this] { return m_exiting || !m_jobQueue.empty(); });

                if (m_exiting)
                    return;

                m_activeThreads++;

                jobIdx = m_jobQueue.front();
                m_jobQueue.pop_front();

                assert(m_jobs[jobIdx].status == JobStatus::Pending);

                m_jobs[jobIdx].status = JobStatus::Executing;
                func = m_jobs[jobIdx].func;
            }

            func();

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_activeThreads--;

                if (m_jobs[jobIdx].hasWaiter)
                {
                    m_jobs[jobIdx].status = JobStatus::Finished;
                }
                else
                {
                    m_jobs[jobIdx].func   = {};
                    m_jobs[jobIdx].status = JobStatus::Free;
                    m_freeJobSlots.push_back(jobIdx);
                }
            }

            m_jobCompletedCV.notify_all();
        }
    }

private:
    bool                     m_exiting = false;
    uint32_t                 m_activeThreads = 0;
    std::vector<std::thread> m_workerThreads;
    std::condition_variable  m_jobAddedCV;
    std::condition_variable  m_jobCompletedCV;
    std::mutex               m_mutex;
    std::deque<uint32_t>     m_jobQueue;
    std::vector<Job>         m_jobs;
    std::vector<int32_t>     m_freeJobSlots;
};

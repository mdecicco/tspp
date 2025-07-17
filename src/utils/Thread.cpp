#define TSPP_INCLUDING_WINDOWS_H

#include <tspp/utils/Thread.h>
#include <utils/Array.hpp>

#ifdef _WIN32
    #include <Windows.h>
#endif

namespace tspp {
    //
    // Thread
    //

    Thread::Thread(const std::function<void()>& entry) {
        m_isRunning = false;
        reset(entry);
    }

    Thread::Thread() {
        m_isRunning = false;
    }

    Thread::~Thread() {
        if (m_thread.joinable()) m_thread.join();
    }

    thread_id Thread::getId() const {
        return std::hash<std::thread::id>{}(m_thread.get_id());
    }

    void Thread::reset(const std::function<void()>& entry) {
        if (m_thread.joinable() && isRunning()) m_thread.join();
        m_thread = std::thread([this, entry](){
            this->m_isRunningMutex.lock();
            this->m_isRunning = true;
            this->m_isRunningMutex.unlock();

            entry();
            
            this->m_isRunningMutex.lock();
            this->m_isRunning = false;
            this->m_isRunningMutex.unlock();
        });
    }

    void Thread::setAffinity(u32 cpuIdx) {
        if (Thread::Current() != getId()) {
            return;
        }

        #ifdef _WIN32
        HANDLE thread = (HANDLE)m_thread.native_handle();
        DWORD_PTR r = SetThreadAffinityMask(thread, DWORD_PTR(1) << cpuIdx);
        if (r == 0) {
            DWORD error = GetLastError();
            if (error) {
                LPVOID lpMsgBuf;
                DWORD bufLen = FormatMessage(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,
                    error,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR)&lpMsgBuf,
                    0, 
                    NULL
                );
                if (bufLen) {
                    LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
                    std::string result(lpMsgStr, lpMsgStr+bufLen);

                    LocalFree(lpMsgBuf);

                    printf("SetThreadAffinityMask failed: %s\n", result.c_str()); 
                }
                else printf("SetThreadAffinityMask failed\n"); 
            }
            else printf("SetThreadAffinityMask failed\n"); 
        }
        #endif
    }

    void Thread::waitForExit() {
        if (!m_thread.joinable() || !isRunning()) return;
        m_thread.join();
    }

    bool Thread::isRunning() {
        m_isRunningMutex.lock();
        bool result = m_isRunning;
        m_isRunningMutex.unlock();
        return result;
    }

    thread_id Thread::Current() {
        return std::hash<std::thread::id>{}(std::this_thread::get_id());
    }

    void Thread::Sleep(u32 ms) {
        std::this_thread::sleep_for(std::chrono::duration<u32, std::milli>(ms));
    }

    u32 Thread::MaxHardwareThreads() {
        return std::thread::hardware_concurrency();
    }

    u32 Thread::CurrentCpuIndex() {
        #ifdef _WIN32
        return GetCurrentProcessorNumber();
        #else
        return 0;
        #endif
    }



    //
    // IJob
    //
    
    IJob::IJob() {}

    IJob::~IJob() {}



    //
    // Worker
    //

    Worker::Worker() : m_id(0), m_doStop(false), m_thread(), m_pool(nullptr) {
    }

    Worker::~Worker() {
        m_doStop = true;
    }

    void Worker::waitForTerminate() {
        m_thread.waitForExit();
    }

    void Worker::start(worker_id id, u32 cpuIdx) {
        m_id = id;
        m_thread.reset([this, cpuIdx]{
            m_thread.setAffinity(cpuIdx);
            run();
        });
    }

    void Worker::run() {
        while (!m_doStop) {
            m_pool->waitForWork(this);

            IJob* j = m_pool->getWork();
            if (j) {
                j->run();
                m_pool->addCompleted(j);
            }
        }
    }



    //
    // ThreadPool
    //
    ThreadPool::ThreadPool() {
        m_workers = nullptr;
    }

    ThreadPool::~ThreadPool() {
        shutdown();
    }

    void ThreadPool::start() {
        if (m_workers) return;

        u32 wc = Thread::MaxHardwareThreads();
        m_workers = new Worker[wc];
        for (u32 i = 0;i < wc;i++) {
            m_workers[i].m_pool = this;
            m_workers[i].start(i + 1, i);
        }
    }

    void ThreadPool::shutdown() {
        if (!m_workers) return;

        u32 wc = Thread::MaxHardwareThreads();
        for (u32 i = 0;i < wc;i++) {
            m_workers[i].m_doStop = true;
        }

        m_workCondition.notify_all();

        
        for (u32 i = 0;i < wc;i++) {
            m_workers[i].waitForTerminate();
        }

        delete [] m_workers;
        m_workers = nullptr;
    }

    void ThreadPool::submitJob(IJob* job) {
        m_jobMutex.lock();
        m_pending.push(job);
        m_jobMutex.unlock();
        m_workCondition.notify_all();
    }
    
    void ThreadPool::submitJobs(const Array<IJob*>& jobs) {
        m_jobMutex.lock();
        m_pending.append(jobs);
        m_jobMutex.unlock();
        m_workCondition.notify_all();
    }

    bool ThreadPool::processCompleted() {
        m_completedMutex.lock();
        bool didHaveWork = m_completed.size() > 0;

        for (IJob* j : m_completed) {
            j->afterComplete();
            delete j;
        }

        m_completed.clear();
        m_completedMutex.unlock();

        if (!didHaveWork) {
            m_jobMutex.lock();
            didHaveWork = m_pending.size() > 0;
            m_jobMutex.unlock();
        }

        return didHaveWork;
    }

    IJob* ThreadPool::getWork() {
        IJob* ret = nullptr;
        m_jobMutex.lock();
        if (m_pending.size() > 0) ret = m_pending.pop();
        m_jobMutex.unlock();

        return ret;
    }

    bool ThreadPool::hasWork() const {
        return m_pending.size() > 0;
    }

    void ThreadPool::waitForWork(Worker* w) {
        if (m_pending.size() > 0) return;
        std::unique_lock<std::mutex> l(m_jobMutex);
        m_workCondition.wait(l, [this, w]{ return w->m_doStop || m_pending.size() > 0; });
    }

    void ThreadPool::addCompleted(IJob* job) {
        m_completedMutex.lock();
        m_completed.push(job);
        m_completedMutex.unlock();
    }
};
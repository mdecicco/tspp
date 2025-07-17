#pragma once
#include <tspp/types.h>
#include <utils/Array.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace tspp {
    typedef u32 thread_id;
    typedef u32 worker_id;

    class Thread {
        public:
            Thread();
            Thread(const std::function<void()>& entry);
            ~Thread();

            thread_id getId() const;
            void reset(const std::function<void()>& entry);
            void setAffinity(u32 cpuIdx);
            void waitForExit();
            bool isRunning();

            static thread_id Current();
            static void Sleep(u32 ms);
            static u32 MaxHardwareThreads();
            static u32 CurrentCpuIndex();

        protected:
            std::thread m_thread;
            std::mutex m_isRunningMutex;
            bool m_isRunning;
    };

    class IJob {
        public:
            IJob();
            virtual ~IJob();

            /**
             * @brief Called when the job is run.
             * 
             * @note This is called from a worker thread.
             */
            virtual void run() = 0;

            /**
             * @brief Called when the job is complete.
             * 
             * @note This is called on the runtime thread.
             */
            virtual void afterComplete() = 0;
    };

    class Worker {
        public:
            void waitForTerminate();

        protected:
            friend class ThreadPool;
            Worker();
            ~Worker();

            void start(worker_id id, u32 cpuIdx);
            void run();

            worker_id m_id;
            bool m_doStop;
            Thread m_thread;
            ThreadPool* m_pool;
    };

    class ThreadPool {
        public:
            ThreadPool();
            ~ThreadPool();

            void start();
            void shutdown();

            void submitJob(IJob* job);
            void submitJobs(const Array<IJob*>& jobs);
            bool processCompleted();

        protected:
            friend class Worker;

            IJob* getWork();
            bool hasWork() const;
            void waitForWork(Worker* w);
            void addCompleted(IJob* job);

        private:
            Worker* m_workers;
            Array<IJob*> m_pending;
            Array<IJob*> m_completed;

            std::mutex m_jobMutex;
            std::mutex m_completedMutex;
            std::condition_variable m_workCondition;
    };

    // TODO
    // - Lock-free queues
    // - Memory pool for all job derived classes
};
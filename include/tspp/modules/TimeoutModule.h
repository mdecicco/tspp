#pragma once
#include <tspp/interfaces/IScriptSystemModule.h>
#include <utils/MemoryPool.h>

#include <chrono>
#include <v8.h>

namespace tspp {
    class Runtime;

    /**
     * @brief Module that provides setTimeout and setInterval functionality
     */
    class TimeoutModule : public IScriptSystemModule {
        public:
            /**
             * @brief Constructs a new timeout module
             *
             * @param scriptSystem The script system this module extends
             */
            TimeoutModule(ScriptSystem* scriptSystem);

            /**
             * @brief Destructor
             */
            ~TimeoutModule() override;

            /**
             * @brief Initializes the binding module
             *
             * Sets up the global require function and registers built-in modules.
             *
             * @return bool True if initialization succeeded
             */
            bool initialize() override;

            /**
             * @brief Shuts down the binding module
             *
             * Cleans up any resources used by the binding module.
             */
            void shutdown() override;

            /**
             * @brief Processes any timeouts or intervals
             */
            void service() override;

            u32 setInterval(
                v8::Isolate* isolate,
                const v8::Local<v8::Function>& function,
                u32 delayMS,
                const Array<v8::Local<v8::Value>>& args,
                bool justOnce = false
            );

            void clearInterval(u32 id);

        private:
            using Clock = std::chrono::high_resolution_clock;

            struct Interval {
                public:
                    u32 id;
                    u32 delayMS;
                    bool justOnce;
                    Clock::time_point nextExecutionAt;
                    u32 argCount;
                    Interval* next;
                    v8::Global<v8::Function> function;
                    v8::Global<v8::Value>* args;
            };

            MemoryPool m_intervalPool;
            Interval* m_intervals;
            u32 m_nextId;
    };
}
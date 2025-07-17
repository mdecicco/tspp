#pragma once
#include <tspp/types.h>
#include <tspp/utils/Thread.h>
#include <tspp/utils/CallContext.h>
#include <utils/String.h>

#include <v8.h>

namespace bind {
    class Function;
}

namespace tspp {
    class Runtime;

    class AsyncCallJob : public IJob {
        public:
            AsyncCallJob(
                bind::Function* target,
                Runtime* runtime
            );
            ~AsyncCallJob() override;

            void run() override;
            void afterComplete() override;

            void setup(void* selfPtr, const v8::FunctionCallbackInfo<v8::Value>& args);

        protected:
            Runtime* m_runtime;
            v8::Isolate* m_isolate;
            bind::Function* m_target;
            CallContext m_callContext;
            void* m_args[16];
            void* m_result;
            bool m_hasException;
            String m_exceptionMsg;
            v8::Global<v8::Promise::Resolver> m_resolver;
    };
}
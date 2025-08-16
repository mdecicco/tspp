#include <tspp/interfaces/IDataMarshaller.h>
#include <tspp/tspp.h>
#include <tspp/utils/AsyncCallJob.h>
#include <tspp/utils/HostObjectManager.h>

#include <bind/DataType.h>
#include <bind/Function.h>
#include <bind/FunctionType.h>

namespace tspp {
    AsyncCallJob::AsyncCallJob(bind::Function* target, Runtime* runtime)
        : m_callContext(runtime->getIsolate(), runtime->getIsolate()->GetCurrentContext()) {
        m_runtime      = runtime;
        m_isolate      = runtime->getIsolate();
        m_target       = target;
        m_hasException = false;
    }

    AsyncCallJob::~AsyncCallJob() {
        m_resolver.Reset();
    }

    void AsyncCallJob::run() {
        try {
            m_target->call(m_result, m_args);
        } catch (const std::exception& e) {
            m_hasException = true;
            m_exceptionMsg = e.what();
        }
    }

    void AsyncCallJob::afterComplete() {
        v8::Local<v8::Context> context = m_isolate->GetCurrentContext();

        bind::FunctionType* sig        = m_target->getSignature();
        bind::DataType* retType        = sig->getReturnType();
        HostObjectManager* retObjMgr   = retType->getUserData<DataTypeUserData>().hostObjectManager;
        const bind::type_meta& retInfo = retType->getInfo();

        v8::Local<v8::Promise::Resolver> resolver = m_resolver.Get(m_isolate);
        m_resolver.Reset();

        if (m_hasException) {
            resolver->Reject(
                context,
                v8::Exception::Error(v8::String::NewFromUtf8(m_isolate, m_exceptionMsg.c_str()).ToLocalChecked())
            );
            return;
        }

        if (retInfo.size > 0) {
            bool retNeedsCopy      = !retObjMgr && retInfo.is_pointer == 0;
            DataTypeUserData& data = retType->getUserData<DataTypeUserData>();

            v8::TryCatch tryCatch(m_isolate);

            v8::Local<v8::Value> retVal = data.marshaller->toV8(m_callContext, m_result, retNeedsCopy, true);

            if (tryCatch.HasCaught()) {
                resolver->Reject(context, tryCatch.Exception());
                return;
            }

            if (retObjMgr) {
                retObjMgr->assignTarget(m_result, retVal.As<v8::Object>());
            }

            resolver->Resolve(context, retVal);
        } else {
            resolver->Resolve(context, v8::Undefined(m_isolate));
        }
    }

    void AsyncCallJob::setup(void* selfPtr, const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Local<v8::Context> context = m_isolate->GetCurrentContext();

        bind::FunctionType* sig        = m_target->getSignature();
        bind::DataType* retType        = sig->getReturnType();
        HostObjectManager* retObjMgr   = retType->getUserData<DataTypeUserData>().hostObjectManager;
        const bind::type_meta& retInfo = retType->getInfo();

        if (retInfo.size > 0) {
            if (retObjMgr) {
                m_result = retObjMgr->preemptiveAlloc();
            } else {
                m_result = m_callContext.alloc(retType);
            }
        }

        u32 argIdx = 0;
        if (selfPtr) {
            m_args[argIdx++] = selfPtr;
        }

        v8::TryCatch tryCatch(m_isolate);

        ConstArrayView<bind::FunctionType::Argument> explicitArgs = m_target->getExplicitArgs();
        for (size_t i = 0; i < explicitArgs.size(); i++) {
            DataTypeUserData& data = explicitArgs[i].type->getUserData<DataTypeUserData>();
            m_args[argIdx++]       = data.marshaller->fromV8(m_callContext, args[i]);

            if (tryCatch.HasCaught()) {
                tryCatch.ReThrow();
                return;
            }
        }

        v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(context).ToLocalChecked();
        m_resolver.Reset(m_isolate, resolver);

        args.GetReturnValue().Set(resolver->GetPromise());
    }
}
#include <tspp/interfaces/IDataMarshaller.h>
#include <tspp/tspp.h>
#include <tspp/utils/AsyncCallJob.h>
#include <tspp/utils/CallContext.h>
#include <tspp/utils/CallProxy.h>
#include <tspp/utils/HostObjectManager.h>

#include <bind/DataType.h>
#include <bind/Function.h>
#include <bind/FunctionType.h>

namespace tspp {
    v8::Local<v8::Function> createCallback(v8::Isolate* isolate, bind::FunctionType* sig, void* funcPtr) {
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        v8::Local<v8::Object> data = v8::Object::New(isolate);
        data->Set(context, v8::String::NewFromUtf8(isolate, "a").ToLocalChecked(), v8::External::New(isolate, sig))
            .ToChecked();
        data->Set(context, v8::String::NewFromUtf8(isolate, "b").ToLocalChecked(), v8::External::New(isolate, funcPtr))
            .ToChecked();

        return v8::Function::New(context, CallbackCallProxy, data, 0, v8::ConstructorBehavior::kThrow).ToLocalChecked();
    }

    void CallbackCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        v8::Local<v8::Object> data = args.Data().As<v8::Object>();
        v8::Local<v8::Value> sigVal =
            data->Get(context, v8::String::NewFromUtf8(isolate, "a").ToLocalChecked()).ToLocalChecked();
        v8::Local<v8::Value> funcPtrVal =
            data->Get(context, v8::String::NewFromUtf8(isolate, "b").ToLocalChecked()).ToLocalChecked();
        bind::FunctionType* sig = (bind::FunctionType*)sigVal.As<v8::Value>().As<v8::External>()->Value();
        void* funcPtr           = funcPtrVal.As<v8::Value>().As<v8::External>()->Value();

        const Array<bind::FunctionType::Argument>& sigArgs = sig->getArgs();
        if (sigArgs.size() != args.Length()) {
            isolate->ThrowException(v8::Exception::RangeError(
                v8::String::NewFromUtf8(isolate, "Invalid number of arguments").ToLocalChecked()
            ));
            return;
        }

        bind::DataType* retType        = sig->getReturnType();
        HostObjectManager* retObjMgr   = retType->getUserData<DataTypeUserData>().hostObjectManager;
        const bind::type_meta& retInfo = retType->getInfo();

        CallContext callCtx(isolate, context);

        void* callArgs[16] = {nullptr};
        void* ret          = nullptr;

        if (retInfo.size > 0) {
            if (retObjMgr) {
                ret = retObjMgr->preemptiveAlloc();
            } else {
                ret = callCtx.alloc(retType);
            }
        }

        v8::TryCatch tryCatch(isolate);

        for (size_t i = 0; i < sigArgs.size(); i++) {
            DataTypeUserData& data = sigArgs[i].type->getUserData<DataTypeUserData>();
            callArgs[i]            = data.marshaller->fromV8(callCtx, args[i]);

            if (tryCatch.HasCaught()) {
                isolate->ThrowException(tryCatch.Exception());
                tryCatch.ReThrow();
                return;
            }
        }

        try {
            sig->call(funcPtr, ret, callArgs);
        } catch (const std::exception& e) {
            isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, e.what()).ToLocalChecked()));
            tryCatch.ReThrow();
            return;
        }

        if (retInfo.size > 0) {
            bool retNeedsCopy      = !retObjMgr && retInfo.is_pointer == 0;
            DataTypeUserData& data = retType->getUserData<DataTypeUserData>();

            v8::Local<v8::Value> retVal = data.marshaller->toV8(callCtx, ret, retNeedsCopy);

            if (tryCatch.HasCaught()) {
                tryCatch.ReThrow();
                return;
            }

            if (retObjMgr) {
                retObjMgr->assignTarget(ret, retVal.As<v8::Object>());
            }

            args.GetReturnValue().Set(retVal);
        }
    }

    void AsyncFunctionCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);

        Runtime* runtime = (Runtime*)isolate->GetData(0);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        bind::Function* target = (bind::Function*)args.Data().As<v8::External>()->Value();

        ConstArrayView<bind::FunctionType::Argument> explicitArgs = target->getExplicitArgs();
        if (explicitArgs.size() != args.Length()) {
            isolate->ThrowException(v8::Exception::RangeError(
                v8::String::NewFromUtf8(isolate, "Invalid number of arguments").ToLocalChecked()
            ));
            return;
        }

        v8::TryCatch tryCatch(isolate);

        AsyncCallJob* job = new AsyncCallJob(target, runtime);
        job->setup(nullptr, args);

        if (tryCatch.HasCaught()) {
            tryCatch.ReThrow();
            delete job;
            return;
        }

        runtime->submitJob(job);
    }

    void AsyncMethodCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);

        Runtime* runtime = (Runtime*)isolate->GetData(0);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        v8::Local<v8::Object> obj = args.This();
        if (obj.IsEmpty()) {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "'this' object is not set").ToLocalChecked())
            );
            return;
        }

        if (obj->IsUndefined()) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "'this' object is undefined").ToLocalChecked()
            ));
            return;
        }

        if (obj->IsNull()) {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "'this' object is null").ToLocalChecked())
            );
            return;
        }

        v8::Local<v8::Data> selfField = obj->GetInternalField(0);
        if (selfField.IsEmpty()) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "'this' object's internal pointer is not set").ToLocalChecked()
            ));
            return;
        }

        u8* objPtr = static_cast<u8*>(selfField.As<v8::Value>().As<v8::External>()->Value());
        if (objPtr == (void*)intptr_t(0xdeadbeef)) {
            isolate->ThrowException(v8::Exception::Error(
                v8::String::NewFromUtf8(isolate, "'this' object has been destroyed").ToLocalChecked()
            ));
            return;
        }

        bind::DataType::Property* prop = (bind::DataType::Property*)args.Data().As<v8::External>()->Value();
        bind::Function* target         = (bind::Function*)prop->address.get();

        objPtr += prop->thisOffset;

        ConstArrayView<bind::FunctionType::Argument> explicitArgs = target->getExplicitArgs();
        if (explicitArgs.size() != args.Length()) {
            isolate->ThrowException(v8::Exception::RangeError(
                v8::String::NewFromUtf8(isolate, "Invalid number of arguments").ToLocalChecked()
            ));
            return;
        }

        v8::TryCatch tryCatch(isolate);

        AsyncCallJob* job = new AsyncCallJob(target, runtime);
        job->setup(objPtr, args);

        if (tryCatch.HasCaught()) {
            tryCatch.ReThrow();
            delete job;
            return;
        }

        runtime->submitJob(job);
    }

    void FunctionCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        bind::Function* target = (bind::Function*)args.Data().As<v8::External>()->Value();

        ConstArrayView<bind::FunctionType::Argument> explicitArgs = target->getExplicitArgs();
        if (explicitArgs.size() != args.Length()) {
            isolate->ThrowException(v8::Exception::RangeError(
                v8::String::NewFromUtf8(isolate, "Invalid number of arguments").ToLocalChecked()
            ));
            return;
        }

        bind::FunctionType* sig        = target->getSignature();
        bind::DataType* retType        = sig->getReturnType();
        HostObjectManager* retObjMgr   = retType->getUserData<DataTypeUserData>().hostObjectManager;
        const bind::type_meta& retInfo = retType->getInfo();

        CallContext callCtx(isolate, context);

        void* callArgs[16] = {nullptr};
        void* ret          = nullptr;

        if (retInfo.size > 0) {
            if (retObjMgr) {
                ret = retObjMgr->preemptiveAlloc();
            } else {
                ret = callCtx.alloc(retType);
            }
        }

        v8::TryCatch tryCatch(isolate);

        for (size_t i = 0; i < explicitArgs.size(); i++) {
            DataTypeUserData& data = explicitArgs[i].type->getUserData<DataTypeUserData>();
            callArgs[i]            = data.marshaller->fromV8(callCtx, args[i]);

            if (tryCatch.HasCaught()) {
                tryCatch.ReThrow();
                return;
            }
        }

        try {
            target->call(ret, callArgs);
        } catch (const std::exception& e) {
            isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, e.what()).ToLocalChecked()));
            tryCatch.ReThrow();
            return;
        }

        if (retInfo.size > 0) {
            bool retNeedsCopy      = !retObjMgr && retInfo.is_pointer == 0;
            DataTypeUserData& data = retType->getUserData<DataTypeUserData>();

            v8::Local<v8::Value> retVal = data.marshaller->toV8(callCtx, ret, retNeedsCopy);

            if (tryCatch.HasCaught()) {
                tryCatch.ReThrow();
                return;
            }

            if (retObjMgr) {
                retObjMgr->assignTarget(ret, retVal.As<v8::Object>());
            }

            args.GetReturnValue().Set(retVal);
        }
    }

    void MethodCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        v8::Local<v8::Object> obj = args.This();
        if (obj.IsEmpty()) {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "'this' object is not set").ToLocalChecked())
            );
            return;
        }

        if (obj->IsUndefined()) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "'this' object is undefined").ToLocalChecked()
            ));
            return;
        }

        if (obj->IsNull()) {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "'this' object is null").ToLocalChecked())
            );
            return;
        }

        v8::Local<v8::Data> selfField = obj->GetInternalField(0);
        if (selfField.IsEmpty()) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "'this' object's internal pointer is not set").ToLocalChecked()
            ));
            return;
        }

        u8* objPtr = static_cast<u8*>(selfField.As<v8::Value>().As<v8::External>()->Value());
        if (objPtr == (void*)intptr_t(0xdeadbeef)) {
            isolate->ThrowException(v8::Exception::Error(
                v8::String::NewFromUtf8(isolate, "'this' object has been destroyed").ToLocalChecked()
            ));
            return;
        }

        bind::DataType::Property* prop = (bind::DataType::Property*)args.Data().As<v8::External>()->Value();
        bind::Function* target         = (bind::Function*)prop->address.get();

        objPtr += prop->thisOffset;

        ConstArrayView<bind::FunctionType::Argument> explicitArgs = target->getExplicitArgs();
        if (explicitArgs.size() != args.Length()) {
            isolate->ThrowException(v8::Exception::RangeError(
                v8::String::NewFromUtf8(isolate, "Invalid number of arguments").ToLocalChecked()
            ));
            return;
        }

        bind::FunctionType* sig        = target->getSignature();
        bind::DataType* retType        = sig->getReturnType();
        const bind::type_meta& retInfo = retType->getInfo();
        HostObjectManager* retObjMgr   = retType->getUserData<DataTypeUserData>().hostObjectManager;

        CallContext callCtx(isolate, context);

        void* callArgs[16] = {nullptr};
        callArgs[0]        = &objPtr;
        void* ret          = nullptr;
        if (retInfo.size > 0) {
            if (retObjMgr) {
                ret = retObjMgr->preemptiveAlloc();
            } else {
                ret = callCtx.alloc(retType);
            }
        }

        v8::TryCatch tryCatch(isolate);

        for (size_t i = 0; i < explicitArgs.size(); i++) {
            DataTypeUserData& data = explicitArgs[i].type->getUserData<DataTypeUserData>();
            callArgs[i + 1]        = data.marshaller->fromV8(callCtx, args[i]);

            if (tryCatch.HasCaught()) {
                tryCatch.ReThrow();
                return;
            }
        }

        try {
            target->call(ret, callArgs);
        } catch (const std::exception& e) {
            isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, e.what()).ToLocalChecked()));
            tryCatch.ReThrow();
            return;
        }

        if (retInfo.size > 0) {
            bool retNeedsCopy           = !retObjMgr && retInfo.is_pointer == 0;
            DataTypeUserData& data      = retType->getUserData<DataTypeUserData>();
            v8::Local<v8::Value> retVal = data.marshaller->toV8(callCtx, ret, retNeedsCopy);

            if (tryCatch.HasCaught()) {
                tryCatch.ReThrow();
                return;
            }

            if (retObjMgr) {
                retObjMgr->assignTarget(ret, retVal.As<v8::Object>());
            }

            args.GetReturnValue().Set(retVal);
        }
    }
}
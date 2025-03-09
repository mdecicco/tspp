#include <tspp/utils/CallProxy.h>
#include <tspp/utils/CallContext.h>
#include <tspp/utils/HostObjectManager.h>
#include <tspp/interfaces/IDataMarshaller.h>

#include <bind/Function.h>
#include <bind/FunctionType.h>
#include <bind/DataType.h>

namespace tspp {
    void FunctionCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        bind::Function* target = (bind::Function*)args.Data().As<v8::External>()->Value();

        ConstArrayView<bind::FunctionType::Argument> explicitArgs = target->getExplicitArgs();
        if (explicitArgs.size() != args.Length()) {
            isolate->ThrowException(
                v8::Exception::RangeError(
                    v8::String::NewFromUtf8(isolate, "Invalid number of arguments").ToLocalChecked()
                )
            );
            return;
        }

        bind::FunctionType* sig = target->getSignature();
        bind::DataType* retType = sig->getReturnType();
        HostObjectManager* retObjMgr = retType->getUserData<DataTypeUserData>().hostObjectManager;
        const bind::type_meta& retInfo = retType->getInfo();

        CallContext callCtx(isolate, context);

        void* callArgs[16] = { nullptr };
        void* ret = nullptr;

        if (retInfo.size > 0) {
            if (retObjMgr) ret = retObjMgr->preemptiveAlloc();
            else ret = callCtx.alloc(retType);
        }

        v8::TryCatch tryCatch(isolate);

        for (size_t i = 0; i < explicitArgs.size(); i++) {
            DataTypeUserData& data = explicitArgs[i].type->getUserData<DataTypeUserData>();
            callArgs[i] = data.marshallingData->fromV8(callCtx, args[i]);

            if (tryCatch.HasCaught()) {
                isolate->ThrowException(tryCatch.Exception());
                return;
            }
        }

        target->call(ret, callArgs);

        if (retInfo.size > 0) {
            bool retNeedsCopy = !retObjMgr && retInfo.is_pointer == 0;
            DataTypeUserData& data = retType->getUserData<DataTypeUserData>();

            v8::Local<v8::Value> retVal = data.marshallingData->toV8(callCtx, ret, retNeedsCopy);

            if (tryCatch.HasCaught()) {
                isolate->ThrowException(tryCatch.Exception());
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
                v8::Exception::TypeError(
                    v8::String::NewFromUtf8(isolate, "'this' object is not set").ToLocalChecked()
                )
            );
            return;
        }

        if (obj->IsUndefined()) {
            isolate->ThrowException(
                v8::Exception::TypeError(
                    v8::String::NewFromUtf8(isolate, "'this' object is undefined").ToLocalChecked()
                )
            );
            return;
        }

        if (obj->IsNull()) {
            isolate->ThrowException(
                v8::Exception::TypeError(
                    v8::String::NewFromUtf8(isolate, "'this' object is null").ToLocalChecked()
                )
            );
            return;
        }

        v8::Local<v8::Data> selfField = obj->GetInternalField(0);
        if (selfField.IsEmpty()) {
            isolate->ThrowException(
                v8::Exception::TypeError(
                    v8::String::NewFromUtf8(isolate, "'this' object's internal pointer is not set").ToLocalChecked()
                )
            );
            return;
        }

        u8* objPtr = static_cast<u8*>(selfField.As<v8::Value>().As<v8::External>()->Value());
        if (objPtr == (void*)intptr_t(0xdeadbeef)) {
            isolate->ThrowException(
                v8::Exception::Error(
                    v8::String::NewFromUtf8(isolate, "'this' object has been destroyed").ToLocalChecked()
                )
            );
            return;
        }

        bind::Function* target = (bind::Function*)args.Data().As<v8::External>()->Value();

        ConstArrayView<bind::FunctionType::Argument> explicitArgs = target->getExplicitArgs();
        if (explicitArgs.size() != args.Length()) {
            isolate->ThrowException(
                v8::Exception::RangeError(
                    v8::String::NewFromUtf8(isolate, "Invalid number of arguments").ToLocalChecked()
                )
            );
            return;
        }

        bind::FunctionType* sig = target->getSignature();
        bind::DataType* retType = sig->getReturnType();
        const bind::type_meta& retInfo = retType->getInfo();
        HostObjectManager* retObjMgr = retType->getUserData<DataTypeUserData>().hostObjectManager;

        CallContext callCtx(isolate, context);

        void* callArgs[16] = { nullptr };
        callArgs[0] = objPtr;
        void* ret = nullptr;
        if (retInfo.size > 0) {
            if (retObjMgr) ret = retObjMgr->preemptiveAlloc();
            else ret = callCtx.alloc(retType);
        }

        v8::TryCatch tryCatch(isolate);

        for (size_t i = 0; i < explicitArgs.size(); i++) {
            DataTypeUserData& data = explicitArgs[i].type->getUserData<DataTypeUserData>();
            callArgs[i + 1] = data.marshallingData->fromV8(callCtx, args[i]);

            if (tryCatch.HasCaught()) {
                isolate->ThrowException(tryCatch.Exception());
                return;
            }
        }

        target->call(ret, callArgs);

        if (retInfo.size > 0) {
            bool retNeedsCopy = !retObjMgr && retInfo.is_pointer == 0;
            DataTypeUserData& data = retType->getUserData<DataTypeUserData>();
            v8::Local<v8::Value> retVal = data.marshallingData->toV8(callCtx, ret, retNeedsCopy);

            if (tryCatch.HasCaught()) {
                isolate->ThrowException(tryCatch.Exception());
                return;
            }

            if (retObjMgr) {
                retObjMgr->assignTarget(ret, retVal.As<v8::Object>());
            }

            args.GetReturnValue().Set(retVal);
        }
    }
}
#include <bind/DataType.h>
#include <tspp/marshalling/FunctionMarshaller.h>
#include <tspp/utils/CallContext.h>
#include <tspp/utils/CallProxy.h>
#include <tspp/utils/Callback.h>

namespace tspp {
    FunctionMarshaller::FunctionMarshaller(bind::DataType* dataType) : IDataMarshaller(dataType) {}

    FunctionMarshaller::~FunctionMarshaller() {}

    bool FunctionMarshaller::canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) {
        return value->IsFunction();
    }

    v8::Local<v8::Value> FunctionMarshaller::convertToV8(
        CallContext& context, void* value, bool valueNeedsCopy, bool isHostReturn
    ) {
        return createCallback(context.getIsolate(), (bind::FunctionType*)m_dataType, value);
    }

    void* FunctionMarshaller::convertFromV8(CallContext& callCtx, const v8::Local<v8::Value>& value) {
        v8::Isolate* isolate           = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();

        const bind::type_meta& meta = m_dataType->getInfo();

        if (!value->IsFunction()) {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Value is not a function").ToLocalChecked())
            );

            return nullptr;
        }

        void* fptr = Callback::Create(isolate, (bind::FunctionType*)m_dataType, value.As<v8::Function>());

        if (!fptr) {
            isolate->ThrowException(
                v8::Exception::Error(v8::String::NewFromUtf8(isolate, "Failed to create closure").ToLocalChecked())
            );

            return nullptr;
        }

        callCtx.addCallback(fptr);
        u8* data        = callCtx.alloc(m_dataType);
        *((void**)data) = fptr;

        return data;
    }
}
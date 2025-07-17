#include <tspp/marshalling/ArrayMarshaller.h>
#include <tspp/utils/CallContext.h>
#include <bind/DataType.h>
#include <utils/Array.hpp>

namespace tspp {
    ArrayMarshaller::ArrayMarshaller(bind::DataType* dataType) : IDataMarshaller(dataType) {
    }

    ArrayMarshaller::~ArrayMarshaller() {
    }

    bool ArrayMarshaller::canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) {
        if (!value->IsArray()) return false;

        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array = value.As<v8::Array>();

        DataTypeUserData& userData = m_dataType->getUserData<DataTypeUserData>();
        bind::DataType* elementType = userData.arrayElementType;

        DataTypeUserData& elementUserData = elementType->getUserData<DataTypeUserData>();
        IDataMarshaller* elementMarshaller = elementUserData.marshaller;

        u32 size = array->Length();

        for (u32 i = 0; i < size; i++) {
            if (!elementMarshaller->canAccept(isolate, array->Get(context, i).ToLocalChecked())) return false;
        }

        return true;
    }

    v8::Local<v8::Value> ArrayMarshaller::convertToV8(CallContext& callCtx, void* value, bool valueNeedsCopy) {
        v8::Isolate* isolate = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();

        DataTypeUserData& userData = m_dataType->getUserData<DataTypeUserData>();
        bind::DataType* elementType = userData.arrayElementType;

        DataTypeUserData& elementUserData = elementType->getUserData<DataTypeUserData>();
        IDataMarshaller* elementMarshaller = elementUserData.marshaller;
        
        Array<u8>* array = (Array<u8>*)(value);
        u8* data = array->data();
        
        v8::Local<v8::Array> out = v8::Array::New(isolate, array->size());
        
        size_t elementSize = elementType->getInfo().size;
        
        // Convert each element
        for (u32 i = 0; i < array->size(); i++) {
            void* element = data + (i * elementSize);

            out->Set(
                context,
                i,
                elementMarshaller->toV8(callCtx, element, valueNeedsCopy)
            ).Check();
        }
        
        return out;
    }

    void* ArrayMarshaller::convertFromV8(CallContext& callCtx, const v8::Local<v8::Value>& value) {
        v8::Isolate* isolate = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();

        DataTypeUserData& userData = m_dataType->getUserData<DataTypeUserData>();
        bind::DataType* elementType = userData.arrayElementType;
        u32 elementSize = elementType->getInfo().size;

        DataTypeUserData& elementUserData = elementType->getUserData<DataTypeUserData>();
        IDataMarshaller* elementMarshaller = elementUserData.marshaller;

        u8* data = callCtx.alloc(m_dataType);
        
        if (!value->IsArray()) {
            Array<u8>::constructUnsafe(data, 0, 0, elementSize);
            isolate->ThrowException(
                v8::Exception::TypeError(
                    v8::String::NewFromUtf8(isolate, "Expected an array").ToLocalChecked()
                )
            );
            
            return data;
        }

        v8::Local<v8::Array> array = value.As<v8::Array>();
        u32 size = array->Length();

        u8* elementData = (u8*)Array<u8>::constructUnsafe(data, size, size, elementSize);
        if (!elementData) return data;

        for (u32 i = 0; i < size; i++) {
            callCtx.setNextAllocation(elementData + (i * elementSize));
            elementMarshaller->fromV8(
                callCtx,
                array->Get(context, i).ToLocalChecked()
            );
            callCtx.setNextAllocation(nullptr);
        }

        return data;
    }
}
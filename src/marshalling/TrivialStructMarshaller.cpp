#include <bind/DataType.h>
#include <tspp/marshalling/TrivialStructMarshaller.h>
#include <tspp/utils/CallContext.h>
#include <utils/Array.hpp>

namespace tspp {
    TrivialStructMarshaller::TrivialStructMarshaller(bind::DataType* dataType) : IDataMarshaller(dataType) {}

    TrivialStructMarshaller::~TrivialStructMarshaller() {}

    bool TrivialStructMarshaller::canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) {
        if (!value->IsObject()) {
            return false;
        }

        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        v8::Local<v8::Object> obj                         = value.As<v8::Object>();
        const Array<bind::DataType::Property>& properties = m_dataType->getProps();
        for (u32 i = 0; i < properties.size(); i++) {
            const bind::DataType::Property& prop = properties[i];
            if (prop.offset < 0) {
                continue;
            }

            const DataTypeUserData& userData = prop.type->getUserData<DataTypeUserData>();

            v8::MaybeLocal<v8::Value> maybePropValue =
                obj->Get(context, v8::String::NewFromUtf8(isolate, prop.name.c_str()).ToLocalChecked());

            if (maybePropValue.IsEmpty()) {
                continue;
            }

            v8::Local<v8::Value> propValue = maybePropValue.ToLocalChecked();
            if (propValue->IsUndefined() || propValue->IsNull()) {
                continue;
            }

            if (!userData.marshaller->canAccept(isolate, propValue)) {
                return false;
            }
        }

        return true;
    }

    v8::Local<v8::Value> TrivialStructMarshaller::convertToV8(
        CallContext& callCtx, void* value, bool valueNeedsCopy, bool isHostReturn
    ) {
        v8::Isolate* isolate           = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();

        v8::Local<v8::Object> obj = v8::Object::New(isolate);

        const Array<bind::DataType::Property>& properties = m_dataType->getProps();
        for (u32 i = 0; i < properties.size(); i++) {
            const bind::DataType::Property& prop = properties[i];
            if (prop.offset < 0) {
                continue;
            }

            const DataTypeUserData& userData = prop.type->getUserData<DataTypeUserData>();

            obj->Set(
                   context,
                   v8::String::NewFromUtf8(isolate, prop.name.c_str()).ToLocalChecked(),
                   userData.marshaller->toV8(callCtx, (u8*)value + prop.offset, valueNeedsCopy)
            )
                .Check();
        }

        return obj;
    }

    void* TrivialStructMarshaller::convertFromV8(CallContext& callCtx, const v8::Local<v8::Value>& value) {
        v8::Isolate* isolate           = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();

        u8* data = callCtx.alloc(m_dataType);

        if (!value->IsObject()) {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Value is not an object").ToLocalChecked())
            );

            return data;
        }

        v8::Local<v8::Object> obj = value.As<v8::Object>();

        const Array<bind::DataType::Property>& properties = m_dataType->getProps();
        for (u32 i = 0; i < properties.size(); i++) {
            const bind::DataType::Property& prop = properties[i];
            if (prop.offset < 0) {
                continue;
            }

            const bind::type_meta& propMeta = prop.type->getInfo();

            const DataTypeUserData& userData = prop.type->getUserData<DataTypeUserData>();

            v8::MaybeLocal<v8::Value> maybePropValue =
                obj->Get(context, v8::String::NewFromUtf8(isolate, prop.name.c_str()).ToLocalChecked());

            if (maybePropValue.IsEmpty()) {
                memset(data + prop.offset, 0, propMeta.size);
                continue;
            }

            v8::Local<v8::Value> propValue = maybePropValue.ToLocalChecked();
            if (propValue->IsUndefined() || propValue->IsNull()) {
                memset(data + prop.offset, 0, propMeta.size);
                continue;
            }

            callCtx.setNextAllocation(data + prop.offset);
            userData.marshaller->fromV8(callCtx, propValue);
            callCtx.setNextAllocation(nullptr);
        }

        return data;
    }
}
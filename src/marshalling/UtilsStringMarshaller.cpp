#include <bind/DataType.h>
#include <tspp/marshalling/UtilsStringMarshaller.h>
#include <tspp/utils/CallContext.h>

namespace tspp {
    UtilsStringMarshaller::UtilsStringMarshaller(bind::DataType* dataType) : IDataMarshaller(dataType) {}

    UtilsStringMarshaller::~UtilsStringMarshaller() {}

    bool UtilsStringMarshaller::canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) {
        return value->IsString();
    }

    v8::Local<v8::Value> UtilsStringMarshaller::convertToV8(
        CallContext& context, void* value, bool valueNeedsCopy, bool isHostReturn
    ) {
        v8::Isolate* isolate        = context.getIsolate();
        const bind::type_meta& meta = m_dataType->getInfo();

        String* str = (String*)value;

        return v8::String::NewFromUtf8(
                   isolate, str->size() > 0 ? str->c_str() : "", v8::NewStringType::kNormal, str->size()
        )
            .ToLocalChecked();
    }

    void* UtilsStringMarshaller::convertFromV8(CallContext& callCtx, const v8::Local<v8::Value>& value) {
        v8::Isolate* isolate           = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();
        u8* data                       = callCtx.alloc(m_dataType);
        const bind::type_meta& meta    = m_dataType->getInfo();

        if (!value->IsString()) {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Value is not a string").ToLocalChecked())
            );

            return new (data) String();
        }

        v8::Local<v8::String> v8Str = value->ToString(context).ToLocalChecked();
        v8::String::Utf8Value utf8Value(isolate, v8Str);

        if (utf8Value.length() > 0) {
            return new (data) String(*utf8Value);
        }

        return new (data) String();
    }
}
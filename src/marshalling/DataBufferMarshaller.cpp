#include <bind/DataType.h>
#include <tspp/builtin/databuffer.h>
#include <tspp/marshalling/DataBufferMarshaller.h>
#include <tspp/utils/CallContext.h>

namespace tspp {
    DataBufferMarshaller::DataBufferMarshaller(bind::DataType* dataType) : IDataMarshaller(dataType) {}

    DataBufferMarshaller::~DataBufferMarshaller() {}

    bool DataBufferMarshaller::canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) {
        return value->IsArrayBuffer();
    }

    v8::Local<v8::Value> DataBufferMarshaller::convertToV8(
        CallContext& context, void* value, bool valueNeedsCopy, bool isHostReturn
    ) {
        v8::Isolate* isolate        = context.getIsolate();
        const bind::type_meta& meta = m_dataType->getInfo();

        builtin::databuffer::DataBuffer* buffer = (builtin::databuffer::DataBuffer*)value;

        v8::Local<v8::ArrayBuffer> arrayBuffer = v8::ArrayBuffer::New(isolate, buffer->size());

        if (buffer->size() > 0) {
            memcpy(arrayBuffer->GetBackingStore()->Data(), buffer->data(), buffer->size());
        }

        return arrayBuffer;
    }

    void* DataBufferMarshaller::convertFromV8(CallContext& callCtx, const v8::Local<v8::Value>& value) {
        v8::Isolate* isolate           = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();
        u8* data                       = callCtx.alloc(m_dataType);
        const bind::type_meta& meta    = m_dataType->getInfo();

        if (!value->IsArrayBuffer()) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "Value is not an ArrayBuffer").ToLocalChecked()
            ));

            return new (data) builtin::databuffer::DataBuffer(0);
        }

        v8::Local<v8::ArrayBuffer> arrayBuffer         = value.As<v8::ArrayBuffer>();
        std::shared_ptr<v8::BackingStore> backingStore = arrayBuffer->GetBackingStore();

        builtin::databuffer::DataBuffer* buffer =
            new (data) builtin::databuffer::DataBuffer(backingStore->ByteLength());

        if (buffer->size() > 0) {
            memcpy(buffer->data(), backingStore->Data(), backingStore->ByteLength());
        }

        return buffer;
    }
}
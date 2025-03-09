#include <tspp/interfaces/IDataMarshaller.h>

namespace tspp {
    IDataMarshaller::IDataMarshaller(bind::DataType* dataType) {
        m_dataType = dataType;
    }

    IDataMarshaller::~IDataMarshaller() {
    }

    v8::Local<v8::Value> IDataMarshaller::toV8(CallContext& context, void* value, bool valueNeedsCopy) {
        return convertToV8(context, value, valueNeedsCopy);
    }

    void* IDataMarshaller::fromV8(CallContext& context, const v8::Local<v8::Value>& value) {
        return convertFromV8(context, value);
    }
}
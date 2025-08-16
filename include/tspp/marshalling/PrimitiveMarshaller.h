#pragma once
#include <tspp/interfaces/IDataMarshaller.h>

namespace tspp {
    class PrimitiveMarshaller : public IDataMarshaller {
        public:
            PrimitiveMarshaller(bind::DataType* dataType);
            ~PrimitiveMarshaller() override;

            bool canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) override;

        protected:
            v8::Local<v8::Value> convertToV8(CallContext& context, void* value, bool valueNeedsCopy, bool isHostReturn)
                override;
            void* convertFromV8(CallContext& context, const v8::Local<v8::Value>& value) override;
            bool m_isBoolean;
    };
}
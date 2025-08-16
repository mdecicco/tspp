#pragma once
#include <tspp/types.h>
#include <v8.h>

namespace bind {
    class DataType;
}

namespace tspp {
    class CallContext;

    class IDataMarshaller {
        public:
            IDataMarshaller(bind::DataType* dataType);
            virtual ~IDataMarshaller();

            v8::Local<v8::Value> toV8(
                CallContext& context, void* value, bool valueNeedsCopy = false, bool isHostReturn = false
            );
            void* fromV8(CallContext& context, const v8::Local<v8::Value>& value);

            virtual bool canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) = 0;

        protected:
            virtual v8::Local<v8::Value> convertToV8(
                CallContext& context, void* value, bool valueNeedsCopy, bool isHostReturn
            )                                                                                    = 0;
            virtual void* convertFromV8(CallContext& context, const v8::Local<v8::Value>& value) = 0;

            bind::DataType* m_dataType;
    };
}
#pragma once
#include <tspp/interfaces/IDataMarshaller.h>

#include <v8.h>

namespace bind {
    class FunctionType;
}

namespace tspp {
    class FunctionMarshaller : public IDataMarshaller {
        public:
            FunctionMarshaller(bind::DataType* dataType);
            ~FunctionMarshaller() override;

            bool canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) override;

        protected:
            v8::Local<v8::Value> convertToV8(CallContext& context, void* value, bool valueNeedsCopy, bool isHostReturn)
                override;
            void* convertFromV8(CallContext& context, const v8::Local<v8::Value>& value) override;
    };
}
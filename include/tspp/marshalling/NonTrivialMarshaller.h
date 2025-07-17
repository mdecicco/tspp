#pragma once
#include <tspp/interfaces/IDataMarshaller.h>

namespace tspp {
    class NonTrivialMarshaller : public IDataMarshaller {
        public:
            NonTrivialMarshaller(bind::DataType* dataType);
            ~NonTrivialMarshaller() override;

            bool canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) override;

        protected:
            v8::Local<v8::Value> convertToV8(CallContext& context, void* value, bool valueNeedsCopy) override;
            void* convertFromV8(CallContext& context, const v8::Local<v8::Value>& value) override;

            v8::Local<v8::Value> convertToV8WithCopy(
                CallContext& context,
                void* value,
                HostObjectManager* objMgr
            );

            v8::Local<v8::Object> wrapHostObject(
                CallContext& context,
                void* value,
                JavaScriptTypeData* typeData
            );
    };
}
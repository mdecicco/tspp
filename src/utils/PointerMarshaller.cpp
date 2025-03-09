#include <tspp/utils/PointerMarshaller.h>
#include <tspp/utils/CallContext.h>
#include <bind/DataType.h>
#include <bind/PointerType.h>

namespace tspp {
    PointerMarshaller::PointerMarshaller(bind::DataType* dataType) : IDataMarshaller(dataType) {
    }

    PointerMarshaller::~PointerMarshaller() {
    }

    bool PointerMarshaller::canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) {
        if (value->IsNull() || value->IsUndefined()) return true;

        const bind::type_meta& meta = m_dataType->getInfo();
        bind::PointerType* ptrType = (bind::PointerType*)m_dataType;
        DataTypeUserData& ud = ptrType->getDestinationType()->getUserData<DataTypeUserData>();
        return ud.marshallingData->canAccept(isolate, value);
    }

    v8::Local<v8::Value> PointerMarshaller::convertToV8(CallContext& context, void* value, bool valueNeedsCopy) {
        v8::Isolate* isolate = context.getIsolate();
        const bind::type_meta& meta = m_dataType->getInfo();

        void* ptr = *((void**)value);

        if (ptr == nullptr) return v8::Null(isolate);
        
        bind::PointerType* ptrType = (bind::PointerType*)m_dataType;
        DataTypeUserData& ud = ptrType->getDestinationType()->getUserData<DataTypeUserData>();

        // TODO:
        // This may need further investigation, but we ignore the valueNeedsCopy flag
        // because if the value being converted is a pointer, then it already has some
        // kind of persistent storage on the host side. For example, if a function returns
        // a structure (one that needs to maintain a connection to the host object), then
        // a copy would be needed because once the call handler returns that structure's
        // stack space will be gone. However, if a function returns a pointer to that
        // structure then it can be assumed that that pointer is safe beyond the call
        // handler's scope.
        //
        // However... There would be nothing stopping the JS side from storing the pointed-to
        // host object beyond the point when the host destroys the object. That's a dangerous
        // situation that will need to be addressed in the future.
        return ud.marshallingData->toV8(context, ptr);
    }

    void* PointerMarshaller::convertFromV8(CallContext& callCtx, const v8::Local<v8::Value>& value) {
        v8::Isolate* isolate = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();

        u8* data = callCtx.alloc(m_dataType);

        if (value->IsNull() || value->IsUndefined()) {
            *((void**)data) = nullptr;
            return data;
        }
        
        const bind::type_meta& meta = m_dataType->getInfo();
        
        bind::PointerType* ptrType = (bind::PointerType*)m_dataType;
        DataTypeUserData& ud = ptrType->getDestinationType()->getUserData<DataTypeUserData>();

        *((void**)data) = ud.marshallingData->fromV8(callCtx, value);

        return data;
    }
}
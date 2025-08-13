#include <bind/DataType.h>
#include <bind/Function.h>
#include <tspp/marshalling/NonTrivialMarshaller.h>
#include <tspp/utils/BindObjectType.h>
#include <tspp/utils/CallContext.h>
#include <tspp/utils/HostObjectManager.h>
#include <tspp/utils/JavaScriptTypeData.h>
#include <utils/Array.hpp>

namespace tspp {
    NonTrivialMarshaller::NonTrivialMarshaller(bind::DataType* dataType) : IDataMarshaller(dataType) {}

    NonTrivialMarshaller::~NonTrivialMarshaller() {}

    bool NonTrivialMarshaller::canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) {
        if (!value->IsObject()) {
            return false;
        }

        v8::Local<v8::Object> obj = value.As<v8::Object>();
        if (obj->InternalFieldCount() < 3) {
            return false;
        }

        bind::DataType* type =
            static_cast<bind::DataType*>(obj->GetInternalField(1).As<v8::Value>().As<v8::External>()->Value());
        if (type != m_dataType) {
            return false;
        }

        void* data = obj->GetInternalField(0).As<v8::Value>().As<v8::External>()->Value();
        if (data == (void*)intptr_t(0xdeadbeef)) {
            return false;
        }

        return true;
    }

    v8::Local<v8::Value> NonTrivialMarshaller::convertToV8(CallContext& callCtx, void* value, bool valueNeedsCopy) {
        v8::Isolate* isolate = callCtx.getIsolate();

        HostObjectManager* objMgr = m_dataType->getUserData<DataTypeUserData>().hostObjectManager;
        if (!objMgr) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(
                    isolate,
                    String::Format(
                        "Unable to convert object of type '%s' to JavaScript object: Object manager not found.",
                        m_dataType->getName().c_str()
                    )
                        .c_str()
                )
                    .ToLocalChecked()
            ));

            return v8::Undefined(isolate);
        }

        v8::Local<v8::Object> obj = objMgr->getTargetIfMapped(isolate, value);
        if (!obj.IsEmpty()) {
            return obj;
        }

        if (valueNeedsCopy) {
            return convertToV8WithCopy(callCtx, value, objMgr);
        }

        JavaScriptTypeData* typeData = m_dataType->getUserData<DataTypeUserData>().javascriptData;
        if (!typeData) {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(
                                             isolate,
                                             String::Format(
                                                 "Unable to convert object of type '%s' to JavaScript object: Bound "
                                                 "type has not been initialized.",
                                                 m_dataType->getName().c_str()
                                             )
                                                 .c_str()
                )
                                             .ToLocalChecked())
            );

            return v8::Undefined(isolate);
        }

        return wrapHostObject(callCtx, value, typeData);
    }

    void* NonTrivialMarshaller::convertFromV8(CallContext& callCtx, const v8::Local<v8::Value>& value) {
        v8::Isolate* isolate           = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();

        if (!value->IsObject()) {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Expected an object").ToLocalChecked())
            );
            return nullptr;
        }

        v8::Local<v8::Object> obj = value.As<v8::Object>();
        if (obj->InternalFieldCount() < 3) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "Object is missing internal fields").ToLocalChecked()
            ));
            return nullptr;
        }

        bind::DataType* type =
            static_cast<bind::DataType*>(obj->GetInternalField(1).As<v8::Value>().As<v8::External>()->Value());

        u32 thisPtrOffset = 0;

        if (type != m_dataType) {
            const Array<bind::DataType::BaseType>& bases = type->getBases();
            const bind::DataType::BaseType* base         = nullptr;

            for (u32 i = 0; i < bases.size(); i++) {
                if (bases[i].type == m_dataType) {
                    base = &bases[i];
                    break;
                }
            }

            if (!base) {
                isolate->ThrowException(v8::Exception::TypeError(
                    v8::String::NewFromUtf8(
                        isolate,
                        String::Format(
                            "Provided object is of type '%s', expected '%s' or a type that inherits from it.",
                            type->getName().c_str(),
                            m_dataType->getName().c_str()
                        )
                            .c_str()
                    )
                        .ToLocalChecked()
                ));

                return nullptr;
            }

            thisPtrOffset = base->offset;
        }

        void* objPtr = obj->GetInternalField(0).As<v8::Value>().As<v8::External>()->Value();
        if (objPtr == (void*)intptr_t(0xdeadbeef)) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(
                    isolate,
                    String::Format("Object of type '%s' has been destroyed.", m_dataType->getName().c_str()).c_str()
                )
                    .ToLocalChecked()
            ));
            return nullptr;
        }

        objPtr = (void*)((u8*)objPtr + thisPtrOffset);

        if (callCtx.hasAllocationTarget()) {
            void* dest               = callCtx.alloc(m_dataType);
            bind::Function* copyCtor = nullptr;
            m_dataType->findConstructors(
                {(bind::DataType*)m_dataType->getPointerType()}, true, bind::FullAccessRights, &copyCtor
            );
            if (!copyCtor) {
                isolate->ThrowException(
                    v8::Exception::TypeError(v8::String::NewFromUtf8(
                                                 isolate,
                                                 String::Format(
                                                     "Unable to convert object of type '%s' from JavaScript object: "
                                                     "Object needs copy and no copy constructor found.",
                                                     m_dataType->getName().c_str()
                                                 )
                                                     .c_str()
                    )
                                                 .ToLocalChecked())
                );

                return nullptr;
            }

            void* args[2] = {&dest, &objPtr};
            copyCtor->call(nullptr, args);

            return dest;
        }

        return objPtr;
    }

    v8::Local<v8::Value> NonTrivialMarshaller::convertToV8WithCopy(
        CallContext& callCtx, void* value, HostObjectManager* objMgr
    ) {
        v8::Isolate* isolate           = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();

        JavaScriptTypeData* typeData = m_dataType->getUserData<DataTypeUserData>().javascriptData;
        if (!typeData) {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(
                                             isolate,
                                             String::Format(
                                                 "Unable to convert object of type '%s' to JavaScript object: Bound "
                                                 "type has not been initialized.",
                                                 m_dataType->getName().c_str()
                                             )
                                                 .c_str()
                )
                                             .ToLocalChecked())
            );

            return v8::Undefined(isolate);
        }

        bind::Function* copyCtor = nullptr;
        m_dataType->findConstructors(
            {(bind::DataType*)m_dataType->getPointerType()}, true, bind::FullAccessRights, &copyCtor
        );
        if (!copyCtor) {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(
                                             isolate,
                                             String::Format(
                                                 "Unable to convert object of type '%s' to JavaScript object: Object "
                                                 "needs copy and no copy constructor found.",
                                                 m_dataType->getName().c_str()
                                             )
                                                 .c_str()
                )
                                             .ToLocalChecked())
            );

            return v8::Undefined(isolate);
        }

        void* copy    = objMgr->preemptiveAlloc();
        void* args[2] = {&copy, &value};
        copyCtor->call(nullptr, args);

        v8::Local<v8::Object> obj = wrapHostObject(callCtx, copy, typeData);
        objMgr->assignTarget(copy, obj);

        return obj;
    }

    v8::Local<v8::Object> NonTrivialMarshaller::wrapHostObject(
        CallContext& callCtx, void* value, JavaScriptTypeData* typeData
    ) {
        v8::Isolate* isolate           = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();

        v8::Local<v8::FunctionTemplate> ctor = typeData->constructor.Get(isolate);
        v8::Local<v8::ObjectTemplate> inst   = ctor->InstanceTemplate();
        v8::Local<v8::Object> obj            = inst->NewInstance(context).ToLocalChecked();

        setInternalFields(isolate, obj, value, m_dataType, true);
        return obj;
    }
}
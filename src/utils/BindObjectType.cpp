#include <tspp/utils/BindObjectType.h>
#include <tspp/interfaces/IDataMarshaller.h>
#include <tspp/utils/CallContext.h>
#include <tspp/utils/CallProxy.h>
#include <tspp/utils/HostObjectManager.h>
#include <tspp/utils/Docs.h>
#include <utils/Array.hpp>
#include <bind/DataType.h>
#include <bind/Function.h>

#include <v8-fast-api-calls.h>

namespace tspp {
    // v8 only supports bool, u8, i32, u32, i64, u64, f32, f64, and void*
    // So if it's i8, u16, or i16, we need to widen them to their 32-bit counterparts
    // (while still treating the access as the correct type)
    template <typename T>
    struct valid_v8_arg {
        using type = std::conditional_t<
            sizeof(T) == 2,
            std::conditional_t<std::is_unsigned_v<T>, u32, i32>,
            std::conditional_t<
                std::is_same_v<T, i8>,
                i32,
                T
            >
        >;
    };

    template <typename T>
    using valid_v8_arg_t = valid_v8_arg<T>::type;

    
    // v8 only supports bool, i32, u32, i64, u64, f32, f64, and void*
    // So if it's i8, u16, or i16, we need to widen them to their 32-bit counterparts
    // (while still treating the access as the correct type)
    template <typename T>
    struct valid_v8_ret {
        using type = std::conditional_t<
            sizeof(T) == 2,
            std::conditional_t<std::is_unsigned_v<T>, u32, i32>,
            std::conditional_t<
                std::is_same_v<T, i8>,
                i32,
                std::conditional_t<
                    std::is_same_v<T, u8>,
                    u32,
                    T
                >
            >
        >;
    };

    template <typename T>
    using valid_v8_ret_t = valid_v8_ret<T>::type;


    /*
     * Callbacks
     */


    template <typename T>
    valid_v8_ret_t<T> get_attr(v8::Local<v8::Object> obj, v8::FastApiCallbackOptions& opts) {
        u8* ptr = (u8*)obj->GetInternalField(0).As<v8::Value>().As<v8::External>()->Value();
        if (ptr == (void*)intptr_t(0xdeadbeef)) {
            // TODO: Doesn't exist in version 11.9.169.4
            // opts.isolate->ThrowException(
            //     v8::Exception::Error(
            //         v8::String::NewFromUtf8(opts.isolate, "'this' object has been destroyed").ToLocalChecked()
            //     )
            // );
            return valid_v8_ret_t<T>(0);
        }

        const bind::DataType::Property* prop = (bind::DataType::Property*)opts.data.As<v8::Value>().As<v8::External>()->Value();
        return (valid_v8_ret_t<T>)(*((T*)(ptr + prop->offset)));
    }

    template <typename T>
    void set_attr(v8::Local<v8::Object> obj, valid_v8_arg_t<T> value, v8::FastApiCallbackOptions& opts) {
        u8* ptr = (u8*)obj->GetInternalField(0).As<v8::Value>().As<v8::External>()->Value();
        if (ptr == (void*)intptr_t(0xdeadbeef)) {
            // TODO: Doesn't exist in version 11.9.169.4
            // opts.isolate->ThrowException(
            //     v8::Exception::Error(
            //         v8::String::NewFromUtf8(opts.isolate, "'this' object has been destroyed").ToLocalChecked()
            //     )
            // );
            return;
        }

        const bind::DataType::Property* prop = (bind::DataType::Property*)opts.data.As<v8::Value>().As<v8::External>()->Value();
        *((T*)(ptr + prop->offset)) = (T)value;
    }

    void slow_getter(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::EscapableHandleScope scope(isolate);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Object> obj = args.This();

        u8* ptr = (u8*)obj->GetInternalField(0).As<v8::Value>().As<v8::External>()->Value();
        if (ptr == (void*)intptr_t(0xdeadbeef)) {
            isolate->ThrowException(
                v8::Exception::Error(
                    v8::String::NewFromUtf8(isolate, "'this' object has been destroyed").ToLocalChecked()
                )
            );
            return;
        }

        const bind::DataType::Property* prop = (bind::DataType::Property*)args.Data().As<v8::External>()->Value();

        bind::DataType* type = prop->type;
        DataTypeUserData& userData = type->getUserData<DataTypeUserData>();

        CallContext callCtx(isolate, context);
        v8::Local<v8::Value> ret = userData.marshaller->toV8(callCtx, ptr + prop->offset);
        args.GetReturnValue().Set(scope.Escape(ret));
    }

    void slow_setter(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Object> obj = args.This();

        u8* ptr = (u8*)obj->GetInternalField(0).As<v8::Value>().As<v8::External>()->Value();
        if (ptr == (void*)intptr_t(0xdeadbeef)) {
            isolate->ThrowException(
                v8::Exception::Error(
                    v8::String::NewFromUtf8(isolate, "'this' object has been destroyed").ToLocalChecked()
                )
            );
            return;
        }

        const bind::DataType::Property* prop = (bind::DataType::Property*)args.Data().As<v8::External>()->Value();

        bind::DataType* type = prop->type;
        DataTypeUserData& userData = type->getUserData<DataTypeUserData>();

        CallContext callCtx(isolate, context);
        callCtx.setNextAllocation(ptr + prop->offset);
        userData.marshaller->fromV8(callCtx, args[0]);
    }

    template <typename T>
    valid_v8_ret_t<T> get_static_attr(v8::Local<v8::Object> obj, v8::FastApiCallbackOptions& opts) {
        const bind::DataType::Property* prop = (bind::DataType::Property*)opts.data.As<v8::Value>().As<v8::External>()->Value();
        T* ptr = (T*)prop->address.get();
        return (valid_v8_ret_t<T>)(*ptr);
    }

    template <typename T>
    void set_static_attr(v8::Local<v8::Object> obj, valid_v8_arg_t<T> value, v8::FastApiCallbackOptions& opts) {
        const bind::DataType::Property* prop = (bind::DataType::Property*)opts.data.As<v8::Value>().As<v8::External>()->Value();
        T* ptr = (T*)prop->address.get();
        *ptr = (T)value;
    }

    void slow_static_getter(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::EscapableHandleScope scope(isolate);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        const bind::DataType::Property* prop = (bind::DataType::Property*)args.Data().As<v8::External>()->Value();
        u8* ptr = (u8*)prop->address.get();

        bind::DataType* type = prop->type;
        DataTypeUserData& userData = type->getUserData<DataTypeUserData>();

        CallContext callCtx(isolate, context);
        v8::Local<v8::Value> ret = userData.marshaller->toV8(callCtx, ptr);
        args.GetReturnValue().Set(scope.Escape(ret));
    }

    void slow_static_setter(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        const bind::DataType::Property* prop = (bind::DataType::Property*)args.Data().As<v8::External>()->Value();
        u8* ptr = (u8*)prop->address.get();

        bind::DataType* type = prop->type;
        DataTypeUserData& userData = type->getUserData<DataTypeUserData>();

        CallContext callCtx(isolate, context);
        callCtx.setNextAllocation(ptr);
        userData.marshaller->fromV8(callCtx, args[0]);
    }

    template <typename T>
    v8::Local<v8::FunctionTemplate> getter(v8::Isolate* isolate, u32 offset, const bind::DataType::Property* prop) {
        static v8::CFunction cb = v8::CFunction::Make(get_attr<T>);
        return v8::FunctionTemplate::New(
            isolate,
            slow_getter,
            v8::External::New(isolate, const_cast<bind::DataType::Property*>(prop)),
            v8::Local<v8::Signature>(),
            0,
            v8::ConstructorBehavior::kThrow,
            v8::SideEffectType::kHasNoSideEffect,
            &cb
        );
    }

    template <typename T>
    v8::Local<v8::FunctionTemplate> setter(v8::Isolate* isolate, u32 offset, const bind::DataType::Property* prop) {
        v8::CFunction cb = v8::CFunction::Make(set_attr<T>);
        return v8::FunctionTemplate::New(
            isolate,
            slow_setter,
            v8::External::New(isolate, const_cast<bind::DataType::Property*>(prop)),
            v8::Local<v8::Signature>(),
            0,
            v8::ConstructorBehavior::kThrow,
            v8::SideEffectType::kHasSideEffectToReceiver,
            &cb
        );
    }

    template <typename T>
    v8::Local<v8::FunctionTemplate> static_getter(v8::Isolate* isolate, const bind::DataType::Property* prop) {
        static v8::CFunction cb = v8::CFunction::Make(get_static_attr<T>);
        return v8::FunctionTemplate::New(
            isolate,
            slow_static_getter,
            v8::External::New(isolate, const_cast<bind::DataType::Property*>(prop)),
            v8::Local<v8::Signature>(),
            0,
            v8::ConstructorBehavior::kThrow,
            v8::SideEffectType::kHasNoSideEffect,
            &cb
        );
    }

    template <typename T>
    v8::Local<v8::FunctionTemplate> static_setter(v8::Isolate* isolate, const bind::DataType::Property* prop) {
        v8::CFunction cb = v8::CFunction::Make(set_static_attr<T>);
        return v8::FunctionTemplate::New(
            isolate,
            slow_static_setter,
            v8::External::New(isolate, const_cast<bind::DataType::Property*>(prop)),
            v8::Local<v8::Signature>(),
            0,
            v8::ConstructorBehavior::kThrow,
            v8::SideEffectType::kHasSideEffect,
            &cb
        );
    }

    void destroyObject(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Object> obj = args.This();

        bool isExternal = obj->GetInternalField(2).As<v8::Value>().As<v8::Boolean>()->BooleanValue(isolate);
        if (isExternal) {
            isolate->ThrowException(
                v8::Exception::Error(
                    v8::String::NewFromUtf8(isolate, "Cannot destroy externally managed object").ToLocalChecked()
                )
            );
            return;
        }
        
        v8::Local<v8::Data> selfField = obj->GetInternalField(0);
        u8* objPtr = static_cast<u8*>(selfField.As<v8::Value>().As<v8::External>()->Value());

        // Check if already destroyed
        if (objPtr == (void*)intptr_t(0xdeadbeef)) {
            isolate->ThrowException(
                v8::Exception::Error(
                    v8::String::NewFromUtf8(isolate, "Object already destroyed").ToLocalChecked()
                )
            );
            return;
        }
        
        // Get the pointer and type
        bind::DataType* type = static_cast<bind::DataType*>(obj->GetInternalField(1).As<v8::Value>().As<v8::External>()->Value());

        HostObjectManager* objMgr = type->getUserData<DataTypeUserData>().hostObjectManager;
        if (!objMgr) {
            isolate->ThrowException(
                v8::Exception::TypeError(
                    v8::String::NewFromUtf8(
                        isolate,
                        String::Format(
                            "Host object manager not found for type '%s'",
                            type->getName().c_str()
                        ).c_str()
                    ).ToLocalChecked()
                )
            );
            return;
        }
        
        // Call destructor if needed
        bind::Function* dtor = type->getDestructor();
        if (dtor) {
            void* args[1] = { &objPtr };
            dtor->call(nullptr, args);
        }
        
        // Free memory
        objMgr->free(objPtr);
        
        // Mark as destroyed
        obj->SetInternalField(0, v8::External::New(isolate, (void*)intptr_t(0xdeadbeef)));
    }
    
    void objectConstructor(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);

        if (!args.IsConstructCall()) {
            isolate->ThrowException(
                v8::Exception::TypeError(
                    v8::String::NewFromUtf8(
                        isolate,
                        "Constructor cannot be called as a function"
                    ).ToLocalChecked()
                )
            );
            return;
        }

        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        bind::DataType* type = (bind::DataType*)args.Data().As<v8::External>()->Value();
        HostObjectManager* objMgr = type->getUserData<DataTypeUserData>().hostObjectManager;
        if (!objMgr) {
            isolate->ThrowException(
                v8::Exception::TypeError(
                    v8::String::NewFromUtf8(
                        isolate,
                        String::Format(
                            "Host object manager not found for type '%s'",
                            type->getName().c_str()
                        ).c_str()
                    ).ToLocalChecked()
                )
            );
            return;
        }

        bind::Function* selectedCtor = nullptr;
        Array<bind::Function*> ctors = type->getConstructors();

        if (ctors.size() == 0) {
            isolate->ThrowException(
                v8::Exception::TypeError(
                    v8::String::NewFromUtf8(
                        isolate,
                        String::Format(
                            "No constructor found for type '%s'",
                            type->getName().c_str()
                        ).c_str()
                    ).ToLocalChecked()
                )
            );
            return;
        }

        for (u32 i = 0; i < ctors.size(); i++) {
            bind::Function* ctor = ctors[i];
            utils::ConstArrayView<bind::FunctionType::Argument> explicitArgs = ctor->getExplicitArgs();
            if (explicitArgs.size() != args.Length()) continue;

            bool canAccept = true;
            for (u32 a = 0; a < explicitArgs.size(); a++) {
                const bind::FunctionType::Argument& arg = explicitArgs[a];

                bind::DataType* argType = arg.type;
                DataTypeUserData& userData = argType->getUserData<DataTypeUserData>();
                if (!userData.marshaller->canAccept(isolate, args[a])) {
                    canAccept = false;
                    break;
                }
            }

            if (canAccept) {
                if (selectedCtor) {
                    isolate->ThrowException(
                        v8::Exception::TypeError(
                            v8::String::NewFromUtf8(
                                isolate,
                                String::Format(
                                    "More than one constructor found for type '%s' that can accept the provided arguments",
                                    type->getName().c_str()
                                ).c_str()
                            ).ToLocalChecked()
                        )
                    );
                    return;
                }

                selectedCtor = ctor;
            }
        }

        if (!selectedCtor) {
            isolate->ThrowException(
                v8::Exception::TypeError(
                    v8::String::NewFromUtf8(
                        isolate,
                        String::Format(
                            "No constructor found for type '%s' that matches the provided arguments",
                            type->getName().c_str()
                        ).c_str()
                    ).ToLocalChecked()
                )
            );
            return;
        }

        v8::Local<v8::Object> obj = args.This();
        void* objPtr = objMgr->alloc(obj);

        bind::FunctionType* sig = selectedCtor->getSignature();
        bind::DataType* retType = sig->getReturnType();

        CallContext callCtx(isolate, context);

        void* callArgs[16] = { nullptr };
        callArgs[0] = &objPtr;

        ConstArrayView<bind::FunctionType::Argument> explicitArgs = selectedCtor->getExplicitArgs();
        for (size_t i = 0; i < explicitArgs.size(); i++) {
            DataTypeUserData& data = explicitArgs[i].type->getUserData<DataTypeUserData>();
            callArgs[i + 1] = data.marshaller->fromV8(callCtx, args[i]);
        }

        selectedCtor->call(nullptr, callArgs);

        setInternalFields(isolate, obj, objPtr, type, false);
    }


    /*
     * Binding
     */


    void setInternalFields(
        v8::Isolate* isolate,
        const v8::Local<v8::Object>& obj,
        void* objPtr,
        bind::DataType* type,
        bool isExternal
    ) {
        obj->SetInternalField(0, v8::External::New(isolate, objPtr));
        obj->SetInternalField(1, v8::External::New(isolate, type));
        obj->SetInternalField(2, v8::Boolean::New(isolate, isExternal));
    }

    void bindProperty(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate> proto, const bind::DataType::Property& p) {
        u32 off = p.offset;

        v8::Local<v8::String> name = v8::String::NewFromUtf8(isolate, p.name.c_str()).ToLocalChecked();
        v8::PropertyAttribute attr = p.flags.can_write == 1 ? v8::PropertyAttribute::None : v8::PropertyAttribute::ReadOnly;
        attr = v8::PropertyAttribute(attr | v8::PropertyAttribute::DontDelete);

        const bind::type_meta& info = p.type->getInfo();
        if (info.is_integral) {
            if (info.is_unsigned) {
                switch (info.size) {
                    case 1: proto->SetAccessorProperty(name, getter<u8 >(isolate, off, &p), setter<u8 >(isolate, off, &p), attr); break;
                    case 2: proto->SetAccessorProperty(name, getter<u16>(isolate, off, &p), setter<u16>(isolate, off, &p), attr); break;
                    case 4: proto->SetAccessorProperty(name, getter<u32>(isolate, off, &p), setter<u32>(isolate, off, &p), attr); break;
                    case 8: proto->SetAccessorProperty(name, getter<u64>(isolate, off, &p), setter<u64>(isolate, off, &p), attr); break;
                }
            } else {
                switch (info.size) {
                    case 1: proto->SetAccessorProperty(name, getter<i8 >(isolate, off, &p), setter<i8 >(isolate, off, &p), attr); break;
                    case 2: proto->SetAccessorProperty(name, getter<i16>(isolate, off, &p), setter<i16>(isolate, off, &p), attr); break;
                    case 4: proto->SetAccessorProperty(name, getter<i32>(isolate, off, &p), setter<i32>(isolate, off, &p), attr); break;
                    case 8: proto->SetAccessorProperty(name, getter<i64>(isolate, off, &p), setter<i64>(isolate, off, &p), attr); break;
                }
            }
        } else if (info.is_floating_point) {
            switch (info.size) {
                case 4: proto->SetAccessorProperty(name, getter<f32>(isolate, off, &p), setter<f32>(isolate, off, &p), attr); break;
                case 8: proto->SetAccessorProperty(name, getter<f64>(isolate, off, &p), setter<f64>(isolate, off, &p), attr); break;
            }
        } else {
            proto->SetAccessorProperty(
                name,
                v8::FunctionTemplate::New(isolate, slow_getter, v8::External::New(isolate, const_cast<bind::DataType::Property*>(&p))),
                v8::FunctionTemplate::New(isolate, slow_setter, v8::External::New(isolate, const_cast<bind::DataType::Property*>(&p))),
                attr
            );
        }
    }

    void bindStaticProperty(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate> proto, const bind::DataType::Property& p) {
        v8::Local<v8::String> name = v8::String::NewFromUtf8(isolate, p.name.c_str()).ToLocalChecked();
        v8::PropertyAttribute attr = p.flags.can_write == 1 ? v8::PropertyAttribute::None : v8::PropertyAttribute::ReadOnly;
        attr = v8::PropertyAttribute(attr | v8::PropertyAttribute::DontDelete);

        const bind::type_meta& info = p.type->getInfo();
        if (info.is_integral) {
            if (info.is_unsigned) {
                switch (info.size) {
                    case 1: proto->SetAccessorProperty(name, static_getter<u8 >(isolate, &p), static_setter<u8 >(isolate, &p), attr); break;
                    case 2: proto->SetAccessorProperty(name, static_getter<u16>(isolate, &p), static_setter<u16>(isolate, &p), attr); break;
                    case 4: proto->SetAccessorProperty(name, static_getter<u32>(isolate, &p), static_setter<u32>(isolate, &p), attr); break;
                    case 8: proto->SetAccessorProperty(name, static_getter<u64>(isolate, &p), static_setter<u64>(isolate, &p), attr); break;
                }
            } else {
                switch (info.size) {
                    case 1: proto->SetAccessorProperty(name, static_getter<i8 >(isolate, &p), static_setter<i8 >(isolate, &p), attr); break;
                    case 2: proto->SetAccessorProperty(name, static_getter<i16>(isolate, &p), static_setter<i16>(isolate, &p), attr); break;
                    case 4: proto->SetAccessorProperty(name, static_getter<i32>(isolate, &p), static_setter<i32>(isolate, &p), attr); break;
                    case 8: proto->SetAccessorProperty(name, static_getter<i64>(isolate, &p), static_setter<i64>(isolate, &p), attr); break;
                }
            }
        } else if (info.is_floating_point) {
            switch (info.size) {
                case 4: proto->SetAccessorProperty(name, static_getter<f32>(isolate, &p), static_setter<f32>(isolate, &p), attr); break;
                case 8: proto->SetAccessorProperty(name, static_getter<f64>(isolate, &p), static_setter<f64>(isolate, &p), attr); break;
            }
        } else {
            proto->SetAccessorProperty(
                name,
                v8::FunctionTemplate::New(isolate, slow_static_getter, v8::External::New(isolate, const_cast<bind::DataType::Property*>(&p))),
                v8::FunctionTemplate::New(isolate, slow_static_setter, v8::External::New(isolate, const_cast<bind::DataType::Property*>(&p))),
                attr
            );
        }
    }

    void bindMethod(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate> proto, const bind::DataType::Property& p) {
        bind::Function* func = (bind::Function*)p.address.get();
        FunctionUserData& userData = func->getUserData<FunctionUserData>();
        FunctionDocumentation* docs = userData.documentation;
        bool isAsync = docs ? docs->isAsync() : false;

        proto->Set(
            v8::String::NewFromUtf8(isolate, p.name.c_str()).ToLocalChecked(),
            v8::FunctionTemplate::New(
                isolate,
                p.flags.is_static == 1 ?
                    (isAsync ? AsyncFunctionCallProxy : FunctionCallProxy) :
                    (isAsync ? AsyncMethodCallProxy : MethodCallProxy),
                v8::External::New(isolate, p.address.get()),
                v8::Local<v8::Signature>(),
                0,
                v8::ConstructorBehavior::kThrow
            )
        );
    }

    v8::Local<v8::FunctionTemplate> buildPrototype(v8::Isolate* isolate, bind::DataType* type) {
        v8::EscapableHandleScope scope(isolate);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        v8::Local<v8::FunctionTemplate> ctor = v8::FunctionTemplate::New(
            isolate,
            objectConstructor,
            v8::External::New(isolate, type)
        );
        ctor->SetClassName(v8::String::NewFromUtf8(isolate, type->getName().c_str()).ToLocalChecked());

        v8::Local<v8::ObjectTemplate> inst = ctor->InstanceTemplate();
        v8::Local<v8::ObjectTemplate> proto = ctor->PrototypeTemplate();

        proto->Set(
            v8::String::NewFromUtf8(isolate, "destroy").ToLocalChecked(),
            v8::FunctionTemplate::New(isolate, destroyObject, v8::External::New(isolate, type))
        );

        inst->SetInternalFieldCount(3);

        const Array<bind::DataType::Property>& props = type->getProps();
        for (u32 i = 0; i < props.size(); i++) {
            const bind::DataType::Property& p = props[i];
            if (p.offset < 0) {
                if (p.flags.is_ctor == 1 || p.flags.is_dtor == 1) continue;

                if (p.flags.is_method == 1 || p.flags.is_pseudo_method == 1) {
                    bindMethod(isolate, proto, p);
                    continue;
                }

                if (p.flags.is_static == 1 && p.address.get()) {
                    bindStaticProperty(isolate, proto, p);
                    continue;
                }

                continue;
            }

            bindProperty(isolate, inst, p);
        }

        return scope.Escape(ctor);
    }
}

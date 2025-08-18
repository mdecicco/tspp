#include <bind/DataType.h>
#include <bind/Registry.hpp>
#include <tspp/marshalling/PrimitiveMarshaller.h>
#include <tspp/utils/CallContext.h>

namespace tspp {
    PrimitiveMarshaller::PrimitiveMarshaller(bind::DataType* dataType) : IDataMarshaller(dataType) {
        m_isBoolean = dataType == bind::Registry::GetType<bool>();
    }

    PrimitiveMarshaller::~PrimitiveMarshaller() {}

    bool PrimitiveMarshaller::canAccept(v8::Isolate* isolate, const v8::Local<v8::Value>& value) {
        if (value->IsBoolean()) {
            return true;
        }
        if (value->IsNumber()) {
            return true;
        }

        return false;
    }

    v8::Local<v8::Value> PrimitiveMarshaller::convertToV8(
        CallContext& context, void* value, bool valueNeedsCopy, bool isHostReturn
    ) {
        v8::Isolate* isolate        = context.getIsolate();
        const bind::type_meta& meta = m_dataType->getInfo();

        if (m_isBoolean) {
            return v8::Boolean::New(isolate, *((bool*)value));
        } else if (meta.is_integral || meta.is_opaque) {
            if (meta.is_unsigned || meta.is_opaque) {
                switch (meta.size) {
                    case 1: return v8::Number::New(isolate, (f64) * ((u8*)value));
                    case 2: return v8::Number::New(isolate, (f64) * ((u16*)value));
                    case 4: return v8::Number::New(isolate, (f64) * ((u32*)value));
                    case 8: return v8::Number::New(isolate, (f64) * ((u64*)value));
                }
            } else {
                switch (meta.size) {
                    case 1: return v8::Number::New(isolate, (f64) * ((i8*)value));
                    case 2: return v8::Number::New(isolate, (f64) * ((i16*)value));
                    case 4: return v8::Number::New(isolate, (f64) * ((i32*)value));
                    case 8: return v8::Number::New(isolate, (f64) * ((i64*)value));
                }
            }
        } else if (meta.is_floating_point) {
            switch (meta.size) {
                case 4: return v8::Number::New(isolate, (f64) * ((f32*)value));
                case 8: return v8::Number::New(isolate, (f64) * ((f64*)value));
            }
        } else if (meta.size == 0) {
            return v8::Undefined(isolate);
        }

        isolate->ThrowException(
            v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Unsupported type").ToLocalChecked())
        );

        return v8::Undefined(isolate);
    }

    void* PrimitiveMarshaller::convertFromV8(CallContext& callCtx, const v8::Local<v8::Value>& value) {
        v8::Isolate* isolate           = callCtx.getIsolate();
        v8::Local<v8::Context> context = callCtx.getContext();
        const bind::type_meta& meta    = m_dataType->getInfo();
        u8* data                       = nullptr;
        if (meta.size > 0) {
            data = callCtx.alloc(m_dataType);
        } else {
            callCtx.setNextAllocation(nullptr);
        }

        if (m_isBoolean) {
            if (!value->IsBoolean()) {
                isolate->ThrowException(v8::Exception::TypeError(
                    v8::String::NewFromUtf8(isolate, "Value is not a boolean").ToLocalChecked()
                ));

                return nullptr;
            }

            *((bool*)data) = value->BooleanValue(isolate);
        } else if (meta.is_integral || meta.is_opaque) {
            f64 num = 0.0;
            if (!value->IsNumber()) {
                if (!meta.is_opaque) {
                    isolate->ThrowException(v8::Exception::TypeError(
                        v8::String::NewFromUtf8(isolate, "Value is not a number").ToLocalChecked()
                    ));

                    return nullptr;
                } else if (!value->IsNull()) {
                    isolate->ThrowException(v8::Exception::TypeError(
                        v8::String::NewFromUtf8(
                            isolate, String::Format("Value is not a valid %s", m_dataType->getName().c_str()).c_str()
                        )
                            .ToLocalChecked()
                    ));

                    return nullptr;
                }
            }

            num = value->NumberValue(context).ToChecked();

            if (meta.is_unsigned || meta.is_opaque) {
                switch (meta.size) {
                    case 1: *((u8*)data) = (u8)num; break;
                    case 2: *((u16*)data) = (u16)num; break;
                    case 4: *((u32*)data) = (u32)num; break;
                    case 8: *((u64*)data) = (u64)num; break;
                }
            } else {
                switch (meta.size) {
                    case 1: *((i8*)data) = (i8)num; break;
                    case 2: *((i16*)data) = (i16)num; break;
                    case 4: *((i32*)data) = (i32)num; break;
                    case 8: *((i64*)data) = (i64)num; break;
                }
            }
        } else if (meta.is_floating_point) {
            if (!value->IsNumber()) {
                isolate->ThrowException(
                    v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Value is not a number").ToLocalChecked())
                );

                return nullptr;
            }

            f64 num = value->NumberValue(context).ToChecked();
            switch (meta.size) {
                case 4: *((f32*)data) = (f32)num; break;
                case 8: *((f64*)data) = num; break;
            }
        } else if (meta.size == 0) {
            return nullptr;
        } else {
            isolate->ThrowException(
                v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Unsupported type").ToLocalChecked())
            );

            return nullptr;
        }

        return data;
    }
}
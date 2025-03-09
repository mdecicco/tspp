#pragma once
#include <tspp/types.h>

#include <v8.h>

namespace tspp {
    void setInternalFields(
        v8::Isolate* isolate,
        const v8::Local<v8::Object>& obj,
        void* objPtr,
        bind::DataType* type,
        bool isExternal
    );

    v8::Local<v8::FunctionTemplate> buildPrototype(v8::Isolate* isolate, bind::DataType* type);
}
#pragma once
#include <tspp/types.h>

#include <v8.h>

namespace bind {
    class FunctionType;
}

namespace tspp {
    v8::Local<v8::Function> createCallback(v8::Isolate* isolate, bind::FunctionType* sig, void* funcPtr);
    void CallbackCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args);
    void AsyncFunctionCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args);
    void AsyncMethodCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args);
    void FunctionCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args);
    void MethodCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args);
}


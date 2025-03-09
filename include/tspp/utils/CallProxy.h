#pragma once
#include <tspp/types.h>

#include <v8.h>

namespace tspp {
    void FunctionCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args);
    void MethodCallProxy(const v8::FunctionCallbackInfo<v8::Value>& args);
}

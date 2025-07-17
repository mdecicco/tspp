#pragma once
#include <tspp/types.h>
#include <v8.h>

namespace tspp::builtin::process {
    void populateEnv(v8::Isolate* isolate, const v8::Local<v8::Object>& env);
    void init();
}
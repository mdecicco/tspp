#pragma once

#include <v8.h>

namespace tspp {
    struct JavaScriptTypeData {
        v8::Global<v8::FunctionTemplate> constructor;
    };
}
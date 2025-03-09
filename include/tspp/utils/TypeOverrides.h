#pragma once
#include <tspp/types.h>
#include <utils/Array.h>
#include <bind/Registry.h>

namespace bind {
    class DataType;

    template <typename X>
    struct TypeResolver<Array<X>> {
        static bind::DataType* Get(size_t nativeHash);
    };
}
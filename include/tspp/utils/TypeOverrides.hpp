#pragma once
#include <bind/DataType.h>
#include <tspp/utils/TypeOverrides.h>
#include <utils/Exception.h>
#include <utils/String.h>

namespace bind {
    template <typename X>
    DataType* TypeResolver<Array<X>>::Get(size_t nativeHash) {
        // Get the element type
        DataType* elementType = Registry::GetType<X>();
        if (!elementType) {
            throw InputException(String::Format(
                "TypeResolver - Could not automatically register array type, element type '%s' has not been registered",
                type_name<X>()
            ));
        }

        DataType* arrayType =
            new DataType(String::Format("Array<%s>", elementType->getName().c_str()), meta<Array<X>>(), nullptr);

        tspp::DataTypeUserData& userData = arrayType->getUserData<tspp::DataTypeUserData>();
        userData.arrayElementType        = elementType;

        Registry::Add(arrayType, nativeHash);

        ObjectTypeBuilder<Array<X>> extend(arrayType);
        extend.dtor();

        return arrayType;
    }
}
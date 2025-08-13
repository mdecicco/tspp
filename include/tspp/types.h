#pragma once
#include <utils/types.h>

#ifndef TSPP_INCLUDING_WINDOWS_H
// Windows.h tramples the global scope, does not play nicely with "namespace bind"
namespace bind {
    class DataType;
}
#endif

namespace tspp {
    using namespace utils;

    /**
     * @brief Configuration options for the script system
     */
    struct ScriptConfig {
        public:
            u64 initialHeapSize = 32 * 1024 * 1024;  // 32MB
            u64 maximumHeapSize = 512 * 1024 * 1024; // 512MB
            u16 debuggerPort    = 9229;              // Default port for the debugger
            bool enableDebugger = false;             // Whether to enable the debugger
    };

    /**
     * @brief Configuration options for the Runtime
     */
    struct RuntimeConfig {
        public:
            // File system options
            const char* scriptRootDirectory = ".";

            // Script system options
            ScriptConfig scriptConfig;
    };

#ifndef TSPP_INCLUDING_WINDOWS_H
    class IDataMarshaller;
    class HostObjectManager;
    class DataTypeDocumentation;
    struct JavaScriptTypeData;
    struct DataTypeUserData {
        public:
            const char* typescriptType;
            IDataMarshaller* marshaller;
            JavaScriptTypeData* javascriptData;
            HostObjectManager* hostObjectManager;

            /**
             * @brief The element type of the array, if this type is an array.
             * If this type is not an array, this will be nullptr.
             */
            bind::DataType* arrayElementType;
            DataTypeDocumentation* documentation;
    };

    class FunctionDocumentation;
    struct FunctionUserData {
        public:
            FunctionDocumentation* documentation;
    };
#endif
};

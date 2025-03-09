#pragma once
#include <utils/types.h>

namespace bind {
    class DataType;
}

namespace tspp {
    using namespace utils;

    /**
     * @brief Configuration options for the script system
     */
    struct ScriptConfig {
        // V8 options
        u64 initialHeapSize = 32 * 1024 * 1024;  // 32MB
        u64 maximumHeapSize = 512 * 1024 * 1024; // 512MB
    };

    /**
     * @brief Configuration options for the Runtime
     */
    struct RuntimeConfig {
        // File system options
        bool watchMode = false;
        const char* rootDirectory = ".";
        
        // Caching options
        bool cacheCompiledOutput = true;
        bool cacheV8Bytecode = true;
        const char* cacheDirectory = "./.tspp_cache";
        
        // Script system options
        ScriptConfig scriptConfig;
    };

    class IDataMarshaller;
    class HostObjectManager;
    struct JavaScriptTypeData;
    struct DataTypeUserData {
        const char* typescriptType;
        IDataMarshaller* marshallingData;
        JavaScriptTypeData* javascriptData;
        HostObjectManager* hostObjectManager;

        /**
         * @brief The element type of the array, if this type is an array.
         * If this type is not an array, this will be nullptr.
         */
        bind::DataType* arrayElementType;
    };
};

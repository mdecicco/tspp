#pragma once
#include <tspp/interfaces/IScriptSystemModule.h>

#include <v8.h>

namespace bind {
    class Namespace;
    class ISymbol;
    class Function;
};

namespace tspp {
    class Runtime;

    /**
     * @brief Module that provides binding functionality between C++ and JavaScript
     * 
     * This module sets up the global require function and manages built-in modules
     * that can be required from JavaScript code.
     */
    class BindingModule : public IScriptSystemModule {
        public:
            /**
             * @brief Constructs a new binding module
             * 
             * @param scriptSystem The script system this module extends
             */
            BindingModule(ScriptSystem* scriptSystem, Runtime* runtime);
            
            /**
             * @brief Destructor
             */
            ~BindingModule() override;
            
            /**
             * @brief Initializes the binding module
             * 
             * Sets up the global require function and registers built-in modules.
             * 
             * @return bool True if initialization succeeded
             */
            bool initialize() override;
            
            /**
             * @brief Shuts down the binding module
             * 
             * Cleans up any resources used by the binding module.
             */
            void shutdown() override;

            /**
             * @brief Commits all bindings to the runtime environment
             */
            void commitBindings();
            
        private:
            void bindBuiltInTypes();
            void processGlobalSymbol(bind::ISymbol* symbol);
            void processNamespace(bind::Namespace* ns);
            v8::Local<v8::Function> processFunction(bind::Function* function);
            v8::Local<v8::Value> processDataType(bind::DataType* dataType);

            // Runtime
            Runtime* m_runtime;
    };
} 
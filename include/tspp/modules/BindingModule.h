#pragma once
#include <tspp/interfaces/IScriptSystemModule.h>

#include <v8.h>

namespace bind {
    class Namespace;
    class ISymbol;
    class Function;
    class ValuePointer;
};

namespace tspp {
    class Runtime;
    class SourceFileBuilder;

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
            void processGlobalSymbol(SourceFileBuilder& dts, bind::ISymbol* symbol);
            void defineFunction(
                v8::Local<v8::Object>& target,
                v8::Local<v8::Context>& context,
                v8::Isolate* isolate,
                bind::Function* function
            );
            void defineDataType(
                v8::Local<v8::Object>& target,
                v8::Local<v8::Context>& context,
                v8::Isolate* isolate,
                bind::DataType* dataType
            );
            void defineValue(
                v8::Local<v8::Object>& target,
                v8::Local<v8::Context>& context,
                v8::Isolate* isolate,
                bind::Namespace* ns,
                bind::ValuePointer* value
            );
            void processNamespace(bind::Namespace* ns);
            v8::Local<v8::Function> processFunction(bind::Function* function);
            v8::Local<v8::Value> processDataType(bind::DataType* dataType);

            struct ImportModule {
                public:
                    bind::Namespace* ns;
                    Array<bind::DataType*> imports;
            };

            void emitNamespace(SourceFileBuilder& dts, bind::Namespace* ns);
            void eniUseDataType(
                utils::Array<ImportModule>& importModules, bind::DataType* dataType, bind::Namespace* ns
            );
            void emitNamespaceImports(SourceFileBuilder& dts, bind::Namespace* ns);
            void emitFunction(SourceFileBuilder& dts, bind::Function* function, bool isWithinNamespace = false);
            void emitFunctionDocs(SourceFileBuilder& dts, bind::Function* function);
            void emitDataType(SourceFileBuilder& dts, bind::DataType* dataType, bool isWithinNamespace = false);
            bind::DataType* resolveType(bind::DataType* dataType);
            String getTypeName(bind::DataType* dataType);
            String getArgList(bind::Function* func);

            // Runtime
            Runtime* m_runtime;
    };
}
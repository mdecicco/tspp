#pragma once
#include <tspp/interfaces/IScriptSystemModule.h>
#include <utils/String.h>

#include <v8.h>

namespace tspp {
    class Runtime;

    /**
     * @brief Module that provides TypeScript compiler functionality
     * 
     * This module loads and initializes the TypeScript compiler,
     * and provides an API to access its functionality.
     */
    class TypeScriptCompilerModule : public IScriptSystemModule {
        public:
            /**
             * @brief Constructs a new TypeScript compiler module
             * 
             * @param scriptSystem The script system this module extends
             * @param runtime The runtime instance
             */
            TypeScriptCompilerModule(ScriptSystem* scriptSystem, Runtime* runtime);
            ~TypeScriptCompilerModule() override;
            
            /**
             * @brief Initializes the TypeScript compiler module
             * 
             * Loads and executes the TypeScript compiler code.
             * 
             * @return True if initialization succeeded
             */
            bool initialize() override;

            /**
             * @brief Called after bindings are loaded
             * 
             * @return True if initialization succeeded
             */
            bool onAfterBindings() override;
            
            /**
             * @brief Shuts down the TypeScript compiler module
             */
            void shutdown() override;
            
            /**
             * @brief Compiles TypeScript code to JavaScript
             * 
             * @param fileName The filename of the TypeScript file to compile
             * @return True if compilation succeeded
             */
            bool compile(const String& fileName);

            /**
             * @brief Compiles TypeScript code to JavaScript according to a tsconfig.json file
             * that should be located in the specified directory
             * 
             * @param path The path to the directory to compile
             * @return True if compilation succeeded
             */
            bool compileDirectory(const String& path);
            
            /**
             * @brief Gets the TypeScript compiler version
             * 
             * @return The TypeScript compiler version
             */
            String getVersion();
            
        private:
            bool loadCompiler();
            bool loadCompilationShims();
            
            // Runtime
            Runtime* m_runtime;
            
            // TypeScript compiler global object
            v8::Global<v8::Object> m_tsCompiler;

            // Compilation functions
            v8::Global<v8::Function> m_compileFuncFactory;
            v8::Global<v8::Function> m_compileFile;
            v8::Global<v8::Function> m_compileDirectory;

            // TypeScript compiler version
            String m_version;
        };
}
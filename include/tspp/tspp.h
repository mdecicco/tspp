#pragma once
#include <tspp/types.h>
#include <tspp/bind.h>
#include <tspp/interfaces/IWithLogging.h>
#include <utils/String.h>

#include <v8.h>

namespace tspp {
    class ScriptSystem;
    class BindingModule;
    class ModuleSystemModule;
    
    /**
     * @brief Main class for the TypeScript runtime environment
     * 
     * The Runtime class manages the complete lifecycle of the TypeScript
     * execution environment, including V8 initialization, TypeScript compilation,
     * module loading, and host API binding.
     */
    class Runtime : public IWithLogging {
        public:
            /**
             * @brief Constructs a new Runtime with the specified configuration
             * 
             * @param config Configuration options for the runtime
             */
            Runtime(const RuntimeConfig& config = RuntimeConfig{});
            
            /**
             * @brief Destructor
             */
            ~Runtime();
            
            /**
             * @brief Initializes the runtime
             * 
             * This sets up V8, loads the TypeScript compiler, and prepares
             * the execution environment.
             * 
             * @return True if initialization succeeded
             */
            bool initialize();
            
            /**
             * @brief Shuts down the runtime
             */
            void shutdown();
            
            /**
             * @brief Executes JavaScript code directly
             * 
             * @param code JavaScript code to execute
             * @param filename Optional filename for source mapping
             * @return The result of the execution
             */
            v8::Local<v8::Value> executeString(const String& code, const String& filename = "<string>");
            
            /**
             * @brief Gets the V8 isolate
             * 
             * @return The V8 isolate
             */
            v8::Isolate* getIsolate();
            
            /**
             * @brief Gets the V8 context
             * 
             * @return The V8 context
             */
            v8::Local<v8::Context> getContext();
            
            /**
             * @brief Registers a built-in module
             * 
             * @param id The module ID
             * @param moduleObject The module object to register
             * @return if registration succeeded
             */
            bool registerBuiltInModule(const String& id, v8::Local<v8::Object> moduleObject);
            
            /**
             * @brief Defines a module from JavaScript code
             * 
             * @param id The module ID (can be empty for anonymous modules)
             * @param dependencies Array of dependency module IDs
             * @param factory The factory function that creates the module
             * @return if definition succeeded
             */
            bool defineModule(
                const String& id, 
                const Array<String>& dependencies, 
                v8::Local<v8::Function> factory
            );
            
            /**
             * @brief Requires a module
             * 
             * @param id The module ID to require
             * @return The module exports
             */
            v8::Local<v8::Value> requireModule(const String& id);

            /**
             * @brief Commits all bindings to the environment
             */
            void commitBindings();
            
        private:
            // Configuration
            RuntimeConfig m_config;
            bool m_initialized = false;
            
            // Systems
            ScriptSystem* m_scriptSystem;

            // Modules
            BindingModule* m_bindingModule;
            ModuleSystemModule* m_moduleSystemModule;
    };
}
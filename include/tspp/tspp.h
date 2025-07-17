#pragma once
#include <tspp/types.h>
#include <tspp/bind.h>
#include <tspp/utils/Thread.h>
#include <tspp/interfaces/IWithLogging.h>
#include <utils/String.h>

#include <v8.h>

namespace tspp {
    class ScriptSystem;
    class BindingModule;
    class ModuleSystemModule;
    class TypeScriptCompilerModule;

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
             * @brief Gets the runtime configuration
             * 
             * @return The runtime configuration
             */
            const RuntimeConfig& getConfig() const;
            
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
             * @brief Executes JavaScript code directly
             * 
             * @param code JavaScript code to execute
             * @param filename Optional filename for source mapping
             * @return The result of the execution, if any
             */
            v8::Local<v8::Value> executeString(const String& code, const String& filename = "<string>");

            /**
             * @brief Commits all bindings to the environment
             */
            void commitBindings();

            /**
             * @brief Builds the project described by the tsconfig.json file
             * in the project root directory
             * 
             * @return True if building succeeded
             */
            bool buildProject();

            /**
             * @brief Submits a job to the thread pool
             * 
             * @param job The job to submit
             */
            void submitJob(IJob* job);

            /**
             * @brief Submits an array of jobs to the thread pool
             * 
             * @param jobs The jobs to submit
             */
            void submitJobs(const Array<IJob*>& jobs);

            /**
             * @brief This function should be called consistently and ideally at regular intervals.
             * Processes any completed jobs, runs v8 microtasks. If this function returns false then
             * then there's no more JS related work to do and the runtime can be terminated if desired.
             * 
             * @return True if there was work to do
             */
            bool service();
            
        private:
            // Configuration
            RuntimeConfig m_config;
            bool m_initialized = false;
            
            // Systems
            ScriptSystem* m_scriptSystem;

            // Modules
            BindingModule* m_bindingModule;
            ModuleSystemModule* m_moduleSystemModule;
            TypeScriptCompilerModule* m_typeScriptCompilerModule;

            // Async
            ThreadPool m_threadPool;
    };
}
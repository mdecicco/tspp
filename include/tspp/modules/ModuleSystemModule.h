#pragma once
#include <unordered_map>

#include <tspp/interfaces/IScriptSystemModule.h>

#include <v8.h>

namespace tspp {
    class Runtime;

    /**
     * @brief Module that provides AMD-compliant module system functionality
     *
     * This module implements the Asynchronous Module Definition (AMD) API,
     * providing define() and require() functions for JavaScript modules.
     */
    class ModuleSystemModule : public IScriptSystemModule {
        public:
            /**
             * @brief Constructs a new module system module
             *
             * @param scriptSystem The script system this module extends
             */
            ModuleSystemModule(ScriptSystem* scriptSystem, Runtime* runtime);

            /**
             * @brief Destructor
             */
            ~ModuleSystemModule() override;

            /**
             * @brief Initializes the module system
             *
             * Sets up the global define() and require() functions.
             *
             * @return True if initialization succeeded
             */
            bool initialize() override;

            /**
             * @brief Shuts down the module system
             *
             * Cleans up any resources used by the module system.
             */
            void shutdown() override;

            /**
             * @brief Registers a built-in module
             *
             * @param id The module ID
             * @param moduleObject The module object to register
             * @return True if registration succeeded
             */
            bool registerBuiltInModule(const String& id, v8::Local<v8::Object> moduleObject);

            /**
             * @brief Defines a module from JavaScript code
             *
             * @param id The module ID (can be empty for anonymous modules)
             * @param dependencies Array of dependency module IDs
             * @param factory The factory function that creates the module
             * @return True if definition succeeded
             */
            bool defineModule(const String& id, const Array<String>& dependencies, v8::Local<v8::Function> factory);

            /**
             * @brief Requires a module
             *
             * @param id The module ID to require
             * @return The module exports
             */
            v8::Local<v8::Value> requireModule(const String& id);

            /**
             * @brief Resolves a module ID relative to a base ID
             *
             * @param id The module ID to resolve
             * @param baseId The base module ID
             * @return The resolved module ID
             */
            String resolveModuleId(const String& id, const String& baseId = "");

        private:
            // Module registry entry
            struct ModuleEntry {
                    enum class State {
                        Registered, // Module is registered but not loaded
                        Loading,    // Module is currently loading (resolving dependencies)
                        Loaded      // Module is fully loaded
                    };

                    v8::Isolate* isolate; // TODO: Remove this
                    State state = State::Registered;
                    v8::Global<v8::Object> exports;
                    v8::Global<v8::Function> factory;
                    Array<String> dependencies;

                    ModuleEntry& operator=(const ModuleEntry& other);
            };

            // Set up the global define() function
            void setupDefineFunction();

            // Set up the global require() function
            void setupRequireFunction();

            // Find the path of a circular dependency
            bool findCircularDependencyPath(
                const String& rootId, const String& currentId, const ModuleEntry* current, Array<String>& path
            );

            // Load a module and its dependencies
            v8::Local<v8::Value> loadModule(const String& id);

            // Create the special module, exports, and require arguments
            void createModuleArgs(
                const String& id,
                v8::Local<v8::Object>& moduleObj,
                v8::Local<v8::Object>& exportsObj,
                v8::Local<v8::Function>& requireFunc
            );

            // Module registry
            std::unordered_map<String, ModuleEntry> m_modules;

            // Global function references
            v8::Global<v8::Function> m_defineFunc;
            v8::Global<v8::Function> m_requireFunc;

            // Runtime
            Runtime* m_runtime;
    };
}
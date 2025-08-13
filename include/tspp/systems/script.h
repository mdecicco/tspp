#pragma once
#include <tspp/types.h>
#include <utils/interfaces/IWithLogging.h>

#include <utils/Array.h>
#include <utils/String.h>

#include <libplatform/libplatform.h>
#include <v8.h>

namespace tspp {
    // Forward declarations
    class IScriptSystemModule;

    /**
     * @brief Manages the V8 JavaScript engine
     *
     * The ScriptSystem class handles V8 initialization, script execution,
     * and provides the foundation for TypeScript compilation and execution.
     */
    class ScriptSystem : public IWithLogging {
        public:
            /**
             * @brief Constructs a new ScriptSystem with the specified configuration
             *
             * @param config Configuration options for the script system
             */
            ScriptSystem(const ScriptConfig& config = ScriptConfig{});

            /**
             * @brief Destructor
             */
            ~ScriptSystem();

            /**
             * @brief Initializes the script system
             *
             * This sets up V8 and prepares the execution environment.
             *
             * @return True if initialization succeeded
             */
            bool initialize();

            /**
             * @brief Adds a module to the script system
             *
             * @param module The module to add
             * @param takeOwnership Whether the script system should take ownership of the module
             */
            void addModule(IScriptSystemModule* module, bool takeOwnership = false);

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
             * @return v8::Isolate* The V8 isolate
             */
            v8::Isolate* getIsolate();

            /**
             * @brief Gets the V8 context
             *
             * @return The V8 context
             */
            v8::Local<v8::Context> getContext();

            /**
             * @brief Called periodically by the runtime, ideally at regular intervals
             */
            void service();

            /**
             * @brief Shuts down the script system
             */
            void shutdown();

        private:
            friend class Runtime;
            void onAfterBindings();

            // Configuration
            ScriptConfig m_config;
            bool m_initialized = false;

            // V8 components
            std::unique_ptr<v8::Platform> m_platform;
            v8::Isolate* m_isolate = nullptr;
            v8::Global<v8::Context> m_context;

            // Modules
            Array<IScriptSystemModule*> m_modules;
            Array<IScriptSystemModule*> m_ownedModules;
    };
}
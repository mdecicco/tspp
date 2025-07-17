#pragma once
#include <tspp/types.h>
#include <tspp/interfaces/IWithLogging.h>

namespace tspp {
    // Forward declarations
    class ScriptSystem;

    /**
     * @brief Interface for script system modules
     * 
     * Modules extend the functionality of the ScriptSystem by providing
     * specific features like console logging, module loading, etc.
     */
    class IScriptSystemModule : public IWithLogging {
        public:
            /**
             * @brief Constructs a new script module
             * 
             * @param scriptSystem The script system this module extends
             * @param name The name of the module for logging
             */
            IScriptSystemModule(ScriptSystem* scriptSystem, const char* logContextName, const char* submoduleName);
            
            /**
             * @brief Virtual destructor
             */
            virtual ~IScriptSystemModule();
            
            /**
             * @brief Initializes the module
             * 
             * This is called during ScriptSystem initialization after
             * the V8 environment is set up.
             * 
             * @return bool True if initialization succeeded
             */
            virtual bool initialize();

            /**
             * @brief Called after bindings are loaded
             * 
             * @return bool True if initialization succeeded
             */
            virtual bool onAfterBindings();
            
            /**
             * @brief Shuts down the module
             * 
             * This is called during ScriptSystem shutdown before
             * the V8 environment is torn down.
             */
            virtual void shutdown();
            
            /**
             * @brief Gets the script system this module extends
             * 
             * @return ScriptSystem* The script system
             */
            ScriptSystem* getScriptSystem() const;

            /**
             * @brief Gets the name of the module
             * 
             * @return const char* The name of the module
             */
            const char* getName() const;
            
        protected:
            ScriptSystem* m_scriptSystem;
            const char* m_name;
    };
} 
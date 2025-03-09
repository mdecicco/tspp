#pragma once
#include <tspp/interfaces/IScriptSystemModule.h>

#include <v8-inspector.h>

namespace tspp {
    /**
     * @brief Module that provides console functionality to scripts
     * 
     * This module sets up the V8 inspector to capture console API calls
     * and forward them to the script system's logging functions.
     */
    class ConsoleModule : public IScriptSystemModule, 
                          public v8_inspector::V8InspectorClient, 
                          public v8_inspector::V8Inspector::Channel
    {
        public:
            /**
             * @brief Constructs a new console module
             * 
             * @param scriptSystem The script system this module extends
             */
            ConsoleModule(ScriptSystem* scriptSystem);
            
            /**
             * @brief Destructor
             */
            ~ConsoleModule() override;
            
            /**
             * @brief Initializes the console module
             * 
             * Sets up the V8 inspector to capture console API calls.
             * 
             * @return bool True if initialization succeeded
             */
            bool initialize() override;
            
            /**
             * @brief Shuts down the console module
             * 
             * Cleans up the V8 inspector.
             */
            void shutdown() override;

            // V8InspectorClient implementation
            void runMessageLoopOnPause(int contextGroupId) override;
            void quitMessageLoopOnPause() override;
            void consoleAPIMessage(int contextGroupId, v8::Isolate::MessageErrorLevel level, 
                                  const v8_inspector::StringView& message,
                                  const v8_inspector::StringView& url,
                                  unsigned lineNumber, unsigned columnNumber,
                                  v8_inspector::V8StackTrace*) override;
            
            // V8Inspector::Channel implementation
            void sendResponse(int callId, std::unique_ptr<v8_inspector::StringBuffer> message) override;
            void sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) override;
            void flushProtocolNotifications() override;
            
        private:
            v8_inspector::StringView createStringView(const String& str);

            // Inspector components
            std::unique_ptr<v8_inspector::V8Inspector> m_inspector;
            std::unique_ptr<v8_inspector::V8InspectorSession> m_inspectorSession;
    };
} 
#pragma once
#include <tspp/builtin/socket_server.h>
#include <tspp/interfaces/IScriptSystemModule.h>

#include <v8-inspector.h>

#include <atomic>
#include <memory>
#include <thread>
#include <utils/Array.h>

namespace tspp {
    /**
     * @brief Module that exposes the V8 inspector protocol over a WebSocket so that
     *        Chrome DevTools / VS Code can attach to running scripts.
     *
     * The module spins up an internal WebSocket server (using the built-in
     * WebSocketServer wrapper) on the port supplied at construction. All incoming
     * messages are forwarded to the V8 inspector session, and all outgoing
     * protocol messages are broadcast to every connected client.
     */
    class DebuggerModule : public IScriptSystemModule,
                           public v8_inspector::V8InspectorClient,
                           public v8_inspector::V8Inspector::Channel,
                           public IWebSocketServerListener {
        public:
            DebuggerModule(ScriptSystem* scriptSystem, u16 port);
            ~DebuggerModule() override;

            // IScriptSystemModule
            bool initialize() override;
            void shutdown() override;
            void service() override;

            // V8InspectorClient -------------------------------------------------
            void runMessageLoopOnPause(i32 contextGroupId) override;
            void quitMessageLoopOnPause() override;
            void
            consoleAPIMessage(i32 contextGroupId, v8::Isolate::MessageErrorLevel level, const v8_inspector::StringView& message, const v8_inspector::StringView& url, unsigned lineNumber, unsigned columnNumber, v8_inspector::V8StackTrace*)
                override;

            // V8Inspector::Channel ---------------------------------------------
            void sendResponse(int callId, std::unique_ptr<v8_inspector::StringBuffer> message) override;
            void sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) override;
            void flushProtocolNotifications() override {}

            // IWebSocketServerListener -----------------------------------------
            void onMessage(WebSocketConnection* connection, const void* data, u64 size) override;
            void onConnect(WebSocketConnection* connection) override;
            void onDisconnect(WebSocketConnection* connection) override;

        private:
            // Helpers -----------------------------------------------------------
            static String stringViewToString(const v8_inspector::StringView& view);
            v8_inspector::StringView createStringView(const String& str);
            void sendToClients(const String& msg);

            // Send cached inspector notifications to a newly connected client
            void flushCached(WebSocketConnection* connection);

            // Cache notifications that occur before any client is attached
            Array<String> m_cachedNotifications;

            // Inspector ---------------------------------------------------------
            std::unique_ptr<v8_inspector::V8Inspector> m_inspector;
            std::unique_ptr<v8_inspector::V8InspectorSession> m_inspectorSession;

            // Networking --------------------------------------------------------
            u16 m_port;
            WebSocketServer* m_socket = nullptr;

            // Pause loop flag
            std::atomic<bool> m_isPaused{false};
    };
}
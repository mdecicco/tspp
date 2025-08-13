#define TSPP_INCLUDING_WINDOWS_H
#include <tspp/modules/DebuggerModule.h>
#include <tspp/systems/script.h>
#include <tspp/types.h>

#include <utils/Array.h>
#include <utils/String.h>

#include <v8.h>

#include <chrono>
#include <sstream>

#define ASIO_STANDALONE
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

using namespace std::chrono_literals;

namespace tspp {
    //////////////////////////////////////////////////////////////////////////
    // Helper functions
    //////////////////////////////////////////////////////////////////////////

    String DebuggerModule::stringViewToString(const v8_inspector::StringView& view) {
        if (view.is8Bit()) {
            String result;
            result.copy(reinterpret_cast<const char*>(view.characters8()), view.length());
            return result;
        }

        // Convert UTF-16 to UTF-8
        std::string utf8;
        utf8.reserve(view.length() * 3);
        const uint16_t* chars = view.characters16();
        for (size_t i = 0; i < view.length(); i++) {
            uint16_t c = chars[i];
            if (c <= 0x7F) {
                utf8.push_back(static_cast<char>(c));
            } else if (c <= 0x7FF) {
                utf8.push_back(static_cast<char>(0xC0 | (c >> 6)));
                utf8.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            } else {
                utf8.push_back(static_cast<char>(0xE0 | (c >> 12)));
                utf8.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                utf8.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
        }
        return String(utf8.c_str());
    }

    v8_inspector::StringView DebuggerModule::createStringView(const String& str) {
        return v8_inspector::StringView(reinterpret_cast<const uint8_t*>(str.c_str()), str.size());
    }

    //////////////////////////////////////////////////////////////////////////
    // Construction / Destruction
    //////////////////////////////////////////////////////////////////////////

    DebuggerModule::DebuggerModule(ScriptSystem* scriptSystem, u16 port)
        : IScriptSystemModule(scriptSystem, "DebuggerModule", "Debugger"), m_port(port) {}

    DebuggerModule::~DebuggerModule() {
        shutdown();
    }

    //////////////////////////////////////////////////////////////////////////
    // IScriptSystemModule
    //////////////////////////////////////////////////////////////////////////

    bool DebuggerModule::initialize() {
        // ------------------------------------------------------------ Inspector
        v8::Isolate* isolate = m_scriptSystem->getIsolate();
        if (!isolate) {
            error("Cannot initialize debugger: Isolate not available");
            return false;
        }

        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> context = m_scriptSystem->getContext();
        v8::Context::Scope context_scope(context);

        m_inspector                      = v8_inspector::V8Inspector::create(isolate, this);
        v8_inspector::StringView ctxName = createStringView(String("TSPP Main Context"));
        m_inspector->contextCreated(v8_inspector::V8ContextInfo(context, 1, ctxName));

        v8_inspector::StringView state = createStringView(String("{}"));
        m_inspectorSession =
            m_inspector->connect(1, this, state, v8_inspector::V8Inspector::ClientTrustLevel::kFullyTrusted);

        // Enable Runtime & Debugger domains so front-ends see breakpoints, etc.
        {
            String msg;
            msg = "{\"id\":1,\"method\":\"Runtime.enable\"}";
            m_inspectorSession->dispatchProtocolMessage(createStringView(msg));
            msg = "{\"id\":2,\"method\":\"Debugger.enable\"}";
            m_inspectorSession->dispatchProtocolMessage(createStringView(msg));
        }

        // -------------------------------------------------------- WebSocket IPC
        m_socket = new WebSocketServer(m_port);
        m_socket->addListener(this);

        // Provide DevTools discovery endpoints (/json, /json/list, /json/version)
        m_socket->setHttpHandler([this](websocketpp::connection_hdl hdl) {
            auto serverPtr        = m_socket->getServer();
            using connection_type = websocketpp::server<websocketpp::config::asio>::connection_type;
            connection_type* con  = static_cast<connection_type*>(serverPtr->get_con_from_hdl(hdl).get());

            std::string uri = con->get_request().get_uri();

            std::string responseBody;
            if (uri == "/json" || uri == "/json/list") {
                std::ostringstream oss;
                oss << "[{";
                oss << "\"id\":\"tspp-1\",";
                oss << "\"title\":\"TSPP Main Context\",";
                oss << "\"type\":\"page\",";
                oss << "\"description\":\"\",";
                oss << "\"webSocketDebuggerUrl\":\"ws://127.0.0.1:" << m_port << "/devtools/page/tspp-1\"";
                oss << "}]";
                responseBody = oss.str();
            } else if (uri == "/json/version") {
                responseBody = "{\"Browser\":\"tspp/1.0\",\"Protocol-Version\":\"1.3\"}";
            } else {
                con->set_status(websocketpp::http::status_code::not_found);
                return;
            }

            con->set_status(websocketpp::http::status_code::ok);
            con->set_body(responseBody);
            con->replace_header("Content-Type", "application/json; charset=UTF-8");
        });

        try {
            m_socket->open();
        } catch (const std::exception& e) {
            error("Failed to open WebSocket server: %s", e.what());
            return false;
        }

        log("Debugger listening on ws://localhost:%d", m_port);
        log("Waiting for debugger to connect before releasing control back to the application...");

        while (m_socket && m_socket->isOpen() && m_socket->getConnections().size() == 0) {
            m_socket->processEvents();
            std::this_thread::sleep_for(1ms);
        }

        return true;
    }

    void DebuggerModule::shutdown() {
        if (m_socket) {
            m_socket->removeListener(this);
            m_socket->close();
            delete m_socket;
            m_socket = nullptr;
        }

        // Clean up inspector
        m_inspectorSession.reset();
        m_inspector.reset();
    }

    void DebuggerModule::service() {
        if (!m_socket) {
            return;
        }

        m_socket->processEvents();
    }

    //////////////////////////////////////////////////////////////////////////
    // V8InspectorClient
    //////////////////////////////////////////////////////////////////////////

    void DebuggerModule::consoleAPIMessage(
        int /*contextGroupId*/,
        v8::Isolate::MessageErrorLevel level,
        const v8_inspector::StringView& message,
        const v8_inspector::StringView& /*url*/,
        unsigned /*lineNumber*/,
        unsigned /*columnNumber*/,
        v8_inspector::V8StackTrace* /*stackTrace*/
    ) {
        String msg = stringViewToString(message);
        switch (level) {
            case v8::Isolate::kMessageDebug: debug("%s", msg.c_str()); break;
            case v8::Isolate::kMessageLog:
            case v8::Isolate::kMessageInfo: log("%s", msg.c_str()); break;
            case v8::Isolate::kMessageWarning: warn("%s", msg.c_str()); break;
            case v8::Isolate::kMessageError: error("%s", msg.c_str()); break;
            default: log("%s", msg.c_str()); break;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // V8Inspector::Channel
    //////////////////////////////////////////////////////////////////////////

    void DebuggerModule::sendResponse(int /*callId*/, std::unique_ptr<v8_inspector::StringBuffer> message) {
        String msg = stringViewToString(message->string());
        sendToClients(msg);
    }

    void DebuggerModule::sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) {
        String msg = stringViewToString(message->string());
        sendToClients(msg);
    }

    //////////////////////////////////////////////////////////////////////////
    // IWebSocketServerListener
    //////////////////////////////////////////////////////////////////////////

    void DebuggerModule::onMessage(WebSocketConnection* /*connection*/, const void* data, u64 size) {
        if (!m_inspectorSession) {
            return;
        }
        String msg;
        msg.copy(reinterpret_cast<const char*>(data), static_cast<size_t>(size));
        m_inspectorSession->dispatchProtocolMessage(createStringView(msg));

        // log("Received message: %s", msg.c_str());
    }

    void DebuggerModule::onConnect(WebSocketConnection* connection) {
        log("Client connected");
        flushCached(connection);
    }

    void DebuggerModule::onDisconnect(WebSocketConnection* connection) {
        log("Client disconnected");
    }

    //////////////////////////////////////////////////////////////////////////
    // Internal helpers
    //////////////////////////////////////////////////////////////////////////

    void DebuggerModule::sendToClients(const String& msg) {
        if (!m_socket) {
            return;
        }
        if (m_socket->getConnections().size() == 0) {
            // No one is listening yet – cache for later
            m_cachedNotifications.push(msg);
        } else {
            m_socket->broadcastText((void*)msg.c_str(), msg.size());
            // log("Sent message to clients: %s", msg.c_str());
        }
    }

    void DebuggerModule::flushCached(WebSocketConnection* connection) {
        for (u32 i = 0; i < m_cachedNotifications.size(); i++) {
            const String& msg = m_cachedNotifications[i];
            connection->sendText((void*)msg.c_str(), msg.size());
        }
        m_cachedNotifications.clear();
    }

    void DebuggerModule::quitMessageLoopOnPause() {
        m_isPaused = false;
    }

    void DebuggerModule::runMessageLoopOnPause(i32 contextGroupId) {
        m_isPaused = true;
        while (m_isPaused && m_socket && m_socket->isOpen()) {
            m_socket->processEvents();
            std::this_thread::sleep_for(1ms);
        }
    }
}
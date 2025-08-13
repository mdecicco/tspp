#include <tspp/modules/ConsoleModule.h>
#include <tspp/systems/script.h>

namespace tspp {
    // Helper function to convert V8 inspector StringView to String
    String stringViewToString(const v8_inspector::StringView& view) {
        if (view.is8Bit()) {
            String result;
            result.copy(reinterpret_cast<const char*>(view.characters8()), view.length());
            return result;
        } else {
            // Convert UTF-16 to UTF-8
            std::string utf8;
            utf8.reserve(view.length() * 3); // Worst case scenario for UTF-16 to UTF-8

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
    }

    ConsoleModule::ConsoleModule(ScriptSystem* scriptSystem) : IScriptSystemModule(scriptSystem, "", "Console") {}

    ConsoleModule::~ConsoleModule() {
        shutdown();
    }

    bool ConsoleModule::initialize() {
        v8::Isolate* isolate = m_scriptSystem->getIsolate();
        if (!isolate) {
            error("Cannot initialize console: Isolate not available");
            return false;
        }

        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> context = m_scriptSystem->getContext();
        v8::Context::Scope context_scope(context);

        // Create V8 inspector
        m_inspector = v8_inspector::V8Inspector::create(isolate, this);

        // Create a context group (using 1 as the context group ID)
        v8_inspector::StringView contextName = createStringView(String("TSPP Main Context"));
        m_inspector->contextCreated(v8_inspector::V8ContextInfo(context, 1, contextName));

        // Create inspector session
        v8_inspector::StringView state = createStringView(String("{}"));
        m_inspectorSession =
            m_inspector->connect(1, this, state, v8_inspector::V8Inspector::ClientTrustLevel::kFullyTrusted);

        // Enable console API
        v8_inspector::StringView msg = createStringView(String("{\"id\":1,\"method\":\"Runtime.enable\"}"));
        m_inspectorSession->dispatchProtocolMessage(msg);

        return true;
    }

    void ConsoleModule::shutdown() {
        // Clean up inspector
        m_inspectorSession.reset();
        m_inspector.reset();
    }

    void ConsoleModule::runMessageLoopOnPause(int contextGroupId) {
        // Not needed for console logging
    }

    void ConsoleModule::quitMessageLoopOnPause() {
        // Not needed for console logging
    }

    void ConsoleModule::consoleAPIMessage(
        int contextGroupId,
        v8::Isolate::MessageErrorLevel level,
        const v8_inspector::StringView& message,
        const v8_inspector::StringView& url,
        unsigned lineNumber,
        unsigned columnNumber,
        v8_inspector::V8StackTrace* stackTrace
    ) {
        String messageStr = stringViewToString(message);

        switch (level) {
            case v8::Isolate::kMessageDebug: debug("%s", messageStr.c_str()); break;
            case v8::Isolate::kMessageLog:
            case v8::Isolate::kMessageInfo: log("%s", messageStr.c_str()); break;
            case v8::Isolate::kMessageWarning: warn("%s", messageStr.c_str()); break;
            case v8::Isolate::kMessageError:
                error("%s", messageStr.c_str());
                // if (stackTrace) {
                //     String stackTraceStr = stringViewToString(stackTrace->toString()->string());
                //     error("%s", stackTraceStr.c_str());
                // }
                break;
            default: log("%s", messageStr.c_str()); break;
        }
    }

    // V8Inspector::Channel implementation
    void ConsoleModule::sendResponse(int callId, std::unique_ptr<v8_inspector::StringBuffer> message) {
        // Not needed for console logging
    }

    void ConsoleModule::sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) {
        // Not needed for console logging
    }

    void ConsoleModule::flushProtocolNotifications() {
        // Not needed for console logging
    }

    v8_inspector::StringView ConsoleModule::createStringView(const String& str) {
        return v8_inspector::StringView(reinterpret_cast<const uint8_t*>(str.c_str()), str.size());
    }
}
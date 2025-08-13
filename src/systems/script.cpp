#include <tspp/interfaces/IScriptSystemModule.h>
#include <tspp/modules/ConsoleModule.h>
#include <tspp/modules/DebuggerModule.h>
#include <tspp/systems/script.h>

#include <utils/Array.hpp>
#include <utils/Exception.h>
#include <utils/String.h>

#include <v8-inspector.h>

#include <filesystem>

namespace tspp {
    // ScriptSystem implementation
    ScriptSystem::ScriptSystem(const ScriptConfig& config)
        : IWithLogging("ScriptSystem"), m_config(config), m_initialized(false) {
        // Add built-in modules
        if (m_config.enableDebugger) {
            addModule(new DebuggerModule(this, m_config.debuggerPort), true);
        } else {
            addModule(new ConsoleModule(this), true);
        }
    }

    ScriptSystem::~ScriptSystem() {
        shutdown();

        // Clean up owned modules
        for (auto module : m_ownedModules) {
            delete module;
        }

        m_ownedModules.clear();
        m_modules.clear();
    }

    void ScriptSystem::addModule(IScriptSystemModule* module, bool takeOwnership) {
        addNestedLogger(module);
        m_modules.push(module);

        if (takeOwnership) {
            m_ownedModules.push(module);
        }
    }

    bool ScriptSystem::initialize() {
        debug("Initializing");

        std::filesystem::path cwd = std::filesystem::current_path();
        if (!v8::V8::InitializeICUDefaultLocation(cwd.string().c_str(), (cwd / "icudtl.dat").string().c_str())) {
            error("Call to V8::InitializeICUDefaultLocation failed");
            return false;
        }

        v8::V8::InitializeExternalStartupData(cwd.string().c_str());

        v8::V8::SetFlagsFromString("--turbo-fast-api-calls");

        // Initialize V8
        m_platform = v8::platform::NewDefaultPlatform();
        v8::V8::InitializePlatform(m_platform.get());
        v8::V8::Initialize();

        // Create a new Isolate with the configured heap size
        v8::Isolate::CreateParams create_params;
        create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();

        // Set initial heap size constraints
        create_params.constraints.ConfigureDefaultsFromHeapSize(m_config.initialHeapSize, m_config.maximumHeapSize);

        // Create the isolate
        m_isolate = v8::Isolate::New(create_params);

        debug("Initialized");
        m_initialized = true;

        bool didFail = false;

        {
            // Create a default context
            v8::Isolate::Scope isolate_scope(m_isolate);
            v8::HandleScope handle_scope(m_isolate);

            v8::Local<v8::Context> context = v8::Context::New(m_isolate);
            m_context.Reset(m_isolate, context);

            // Initialize modules
            for (u32 i = 0; i < m_modules.size(); i++) {
                debug("- Initializing submodule: %s", m_modules[i]->getName());

                if (!m_modules[i]->initialize()) {
                    error("Failed to initialize submodule: %s", m_modules[i]->getName());

                    debug("Shutting down");

                    for (i32 j = i32(i) - 1; j >= 0; j--) {
                        debug("- Shutting down submodule: %s", m_modules[j]->getName());
                        m_modules[j]->shutdown();
                    }

                    didFail = true;
                    break;
                }
            }
        }

        if (didFail) {
            // Release the persistent context
            m_context.Reset();

            // Clean up V8
            m_isolate->Dispose();
            v8::V8::Dispose();
            v8::V8::DisposePlatform();

            debug("Shut down successfully");
            m_initialized = false;
            return false;
        }

        return true;
    }

    v8::Local<v8::Value> ScriptSystem::executeString(const String& code, const String& filename) {
        if (!m_initialized) {
            error("Cannot execute script: ScriptSystem not initialized");
            return v8::Local<v8::Value>();
        }

        try {
            v8::Isolate::Scope isolate_scope(m_isolate);
            v8::EscapableHandleScope handle_scope(m_isolate);

            v8::Local<v8::Context> context = m_context.Get(m_isolate);
            v8::Context::Scope context_scope(context);

            // Create a string containing the JavaScript source code
            v8::Local<v8::String> source = v8::String::NewFromUtf8(m_isolate, code.c_str()).ToLocalChecked();

            // Compile the source code
            v8::Local<v8::Script> script;
            v8::ScriptOrigin origin(m_isolate, v8::String::NewFromUtf8(m_isolate, filename.c_str()).ToLocalChecked());

            v8::TryCatch try_catch(m_isolate);

            if (!v8::Script::Compile(context, source, &origin).ToLocal(&script)) {
                v8::String::Utf8Value msg(m_isolate, try_catch.Exception());
                error("Compilation error: %s", *msg);

                v8::Local<v8::Value> stackTrace;
                if (try_catch.StackTrace(context).ToLocal(&stackTrace) && !stackTrace.IsEmpty()) {
                    v8::Local<v8::String> stackTraceStr;
                    if (stackTrace->ToString(context).ToLocal(&stackTraceStr) && !stackTraceStr.IsEmpty()) {
                        v8::String::Utf8Value trace(m_isolate, stackTraceStr);
                        error(*trace);
                    }
                }

                return v8::Local<v8::Value>();
            }

            // Run the script
            v8::Local<v8::Value> result;
            if (!script->Run(context).ToLocal(&result)) {
                v8::String::Utf8Value msg(m_isolate, try_catch.Exception());
                error(*msg);

                v8::Local<v8::Value> stackTrace;
                if (try_catch.StackTrace(context).ToLocal(&stackTrace) && !stackTrace.IsEmpty()) {
                    v8::Local<v8::String> stackTraceStr;
                    if (stackTrace->ToString(context).ToLocal(&stackTraceStr) && !stackTraceStr.IsEmpty()) {
                        v8::String::Utf8Value trace(m_isolate, stackTraceStr);
                        error(*trace);
                    }
                }

                return v8::Local<v8::Value>();
            }

            return handle_scope.Escape(result);
        } catch (const GenericException& e) {
            error("Exception during script execution: %s", e.what());
        }

        return v8::Local<v8::Value>();
    }

    v8::Isolate* ScriptSystem::getIsolate() {
        return m_isolate;
    }

    v8::Local<v8::Context> ScriptSystem::getContext() {
        return m_context.Get(m_isolate);
    }

    void ScriptSystem::service() {
        for (auto module : m_modules) {
            module->service();
        }
    }

    void ScriptSystem::shutdown() {
        if (!m_initialized) {
            return;
        }
        debug("Shutting down");

        // Shut down modules in reverse order
        for (int i = m_modules.size() - 1; i >= 0; i--) {
            debug("- Shutting down submodule: %s", m_modules[i]->getName());
            m_modules[i]->shutdown();
        }

        // Release the persistent context
        m_context.Reset();

        // Clean up V8
        m_isolate->Dispose();
        v8::V8::Dispose();
        v8::V8::DisposePlatform();

        debug("Shut down successfully");
        m_initialized = false;
    }

    void ScriptSystem::onAfterBindings() {
        for (u32 i = 0; i < m_modules.size(); i++) {
            m_modules[i]->onAfterBindings();
        }
    }
}
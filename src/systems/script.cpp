#include <tspp/systems/script.h>
#include <tspp/interfaces/IScriptSystemModule.h>
#include <tspp/modules/ConsoleModule.h>

#include <utils/Exception.h>
#include <utils/String.h>
#include <utils/Array.hpp>

#include <v8-inspector.h>

namespace tspp {
    // ScriptSystem implementation
    ScriptSystem::ScriptSystem(const ScriptConfig& config) 
        : IWithLogging("ScriptSystem"), m_config(config), m_initialized(false)
    {
        // Add built-in modules
        addModule(new ConsoleModule(this), true);
    }

    ScriptSystem::~ScriptSystem() {
        shutdown();
        
        // Clean up owned modules
        for (auto module : m_ownedModules) delete module;

        m_ownedModules.clear();
        m_modules.clear();
    }
    
    void ScriptSystem::addModule(IScriptSystemModule* module, bool takeOwnership) {
        module->addLogHandler(this);
        m_modules.push(module);

        if (takeOwnership) {
            m_ownedModules.push(module);
        }
    }

    bool ScriptSystem::initialize() {
        logDebug("Initializing");
        
        v8::V8::SetFlagsFromString("--turbo-fast-api-calls");

        // Initialize V8
        m_platform = v8::platform::NewDefaultPlatform();
        v8::V8::InitializePlatform(m_platform.get());
        v8::V8::Initialize();
        
        // Create a new Isolate with the configured heap size
        v8::Isolate::CreateParams create_params;
        create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
        
        // Set initial heap size constraints
        create_params.constraints.set_max_old_generation_size_in_bytes(m_config.maximumHeapSize);
        create_params.constraints.set_max_young_generation_size_in_bytes(m_config.initialHeapSize);
        
        // Create the isolate
        m_isolate = v8::Isolate::New(create_params);

        bool didFail = false;
        
        {
            // Create a default context
            v8::Isolate::Scope isolate_scope(m_isolate);
            v8::HandleScope handle_scope(m_isolate);
            
            v8::Local<v8::Context> context = v8::Context::New(m_isolate);
            m_context.Reset(m_isolate, context);
            
            // Initialize modules
            for (u32 i = 0;i < m_modules.size();i++) {
                logDebug("- Initializing submodule: %s", m_modules[i]->getName());

                if (!m_modules[i]->initialize()) {
                    logError("Failed to initialize submodule: %s", m_modules[i]->getName());

                    logDebug("Shutting down");

                    for (i32 j = i32(i) - 1;j >= 0;j--) {
                        logDebug("- Shutting down submodule: %s", m_modules[j]->getName());
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

            logDebug("Shut down successfully");
            return false;
        }
        
        logDebug("Initialized");
        m_initialized = true;
        return true;
    }

    v8::Local<v8::Value> ScriptSystem::executeString(const String& code, const String& filename) {
        if (!m_initialized) {
            logError("Cannot execute script: ScriptSystem not initialized");
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
            v8::ScriptOrigin origin(v8::String::NewFromUtf8(m_isolate, filename.c_str()).ToLocalChecked());
            
            v8::TryCatch try_catch(m_isolate);
            
            if (!v8::Script::Compile(context, source, &origin).ToLocal(&script)) {
                v8::String::Utf8Value error(m_isolate, try_catch.Exception());
                logError("Compilation error: %s", *error);
                return v8::Local<v8::Value>();
            }
            
            // Run the script
            v8::Local<v8::Value> result;
            if (!script->Run(context).ToLocal(&result)) {
                v8::String::Utf8Value error(m_isolate, try_catch.Exception());
                logError("Execution error: %s", *error);
                return v8::Local<v8::Value>();
            }
            
            return handle_scope.Escape(result);
        } catch (const utils::Exception& e) {
            logError("Exception during script execution: %s", e.what());
        }
        
        return v8::Local<v8::Value>();
    }

    v8::Isolate* ScriptSystem::getIsolate() {
        return m_isolate;
    }

    v8::Local<v8::Context> ScriptSystem::getContext() {
        return m_context.Get(m_isolate);
    }

    void ScriptSystem::shutdown() {
        if (!m_initialized) return;
        logDebug("Shutting down");

        // Shut down modules in reverse order
        for (int i = m_modules.size() - 1; i >= 0; i--) {
            logDebug("- Shutting down submodule: %s", m_modules[i]->getName());
            m_modules[i]->shutdown();
        }
        
        // Release the persistent context
        m_context.Reset();
        
        // Clean up V8
        m_isolate->Dispose();
        v8::V8::Dispose();
        v8::V8::DisposePlatform();
        
        logDebug("Shut down successfully");
        m_initialized = false;
    }
} 
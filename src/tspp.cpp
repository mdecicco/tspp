#include <tspp/tspp.h>
#include <tspp/systems/script.h>
#include <tspp/modules/BindingModule.h>
#include <tspp/modules/ModuleSystemModule.h>

#include <tspp/builtin/fs.h>

namespace tspp {
    Runtime::Runtime(const RuntimeConfig& config) : IWithLogging("TSPP") {
        m_config = config;
        m_initialized = false;
        m_scriptSystem = nullptr;
        m_bindingModule = nullptr;
        m_moduleSystemModule = nullptr;
    }

    Runtime::~Runtime() {
        shutdown();
    }

    bool Runtime::initialize() {
        logDebug("Initializing");

        m_scriptSystem = new ScriptSystem(m_config.scriptConfig);
        m_scriptSystem->addLogHandler(this);

        // Create and add the module system first (other modules may depend on it)
        m_moduleSystemModule = new ModuleSystemModule(m_scriptSystem, this);
        m_scriptSystem->addModule(m_moduleSystemModule, true);
        
        // Create and add the binding module
        m_bindingModule = new BindingModule(m_scriptSystem, this);
        m_scriptSystem->addModule(m_bindingModule, true);
        
        // Initialize script system
        if (!m_scriptSystem->initialize()) {
            logError("Failed to initialize");

            delete m_scriptSystem;
            m_scriptSystem = nullptr;

            return false;
        }

        builtin::fs::init();
        
        m_initialized = true;
        logDebug("Initialized");
        return true;
    }

    void Runtime::shutdown() {
        if (!m_initialized) return;
        logDebug("Shutting down");

        // Shut down script system
        m_scriptSystem->shutdown();
        delete m_scriptSystem;
        m_scriptSystem = nullptr;
        
        m_initialized = false;
        logDebug("Shut down successfully");
    }

    v8::Local<v8::Value> Runtime::executeString(const String& code, const String& filename) {
        return m_scriptSystem->executeString(code, filename);
    }
    
    v8::Isolate* Runtime::getIsolate() {
        return m_scriptSystem->getIsolate();
    }
    
    v8::Local<v8::Context> Runtime::getContext() {
        return m_scriptSystem->getContext();
    }
    
    bool Runtime::registerBuiltInModule(const String& id, v8::Local<v8::Object> moduleObject) {
        return m_moduleSystemModule->registerBuiltInModule(id, moduleObject);
    }

    bool Runtime::defineModule(
        const String& id, 
        const Array<String>& dependencies, 
        v8::Local<v8::Function> factory
    ) {
        return m_moduleSystemModule->defineModule(id, dependencies, factory);
    }

    v8::Local<v8::Value> Runtime::requireModule(const String& id) {
        return m_moduleSystemModule->requireModule(id);
    }

    void Runtime::commitBindings() {
        m_bindingModule->commitBindings();
    }
}
#include <tspp/tspp.h>
#include <tspp/systems/script.h>
#include <tspp/utils/Callback.h>
#include <tspp/modules/BindingModule.h>
#include <tspp/modules/ModuleSystemModule.h>
#include <tspp/modules/TypeScriptCompilerModule.h>
#include <tspp/builtin/databuffer.h>
#include <tspp/builtin/fs.h>
#include <tspp/builtin/process.h>
#include <tspp/builtin/path.h>

namespace tspp {
    Runtime::Runtime(const RuntimeConfig& config) : IWithLogging("TSPP") {
        m_config = config;
        m_initialized = false;
        m_scriptSystem = nullptr;
        m_bindingModule = nullptr;
        m_moduleSystemModule = nullptr;
        m_typeScriptCompilerModule = nullptr;
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

        // Create and add the TypeScript compiler module
        m_typeScriptCompilerModule = new TypeScriptCompilerModule(m_scriptSystem, this);
        m_scriptSystem->addModule(m_typeScriptCompilerModule, true);
        
        // Initialize script system
        if (!m_scriptSystem->initialize()) {
            logError("Failed to initialize");

            delete m_scriptSystem;
            m_scriptSystem = nullptr;

            return false;
        }

        m_threadPool.start();

        builtin::databuffer::init();
        builtin::fs::init();
        builtin::process::init();
        builtin::path::init();

        m_scriptSystem->getIsolate()->SetData(0, this);

        m_initialized = true;
        logDebug("Initialized");
        return true;
    }

    void Runtime::shutdown() {
        if (!m_initialized) return;
        logDebug("Shutting down");

        m_threadPool.shutdown();

        Callback::DestroyAll();

        // Shut down script system
        m_scriptSystem->shutdown();
        delete m_scriptSystem;
        m_scriptSystem = nullptr;
        
        m_initialized = false;
        logDebug("Shut down successfully");
    }

    v8::Isolate* Runtime::getIsolate() {
        return m_scriptSystem->getIsolate();
    }
    
    v8::Local<v8::Context> Runtime::getContext() {
        return m_scriptSystem->getContext();
    }

    const RuntimeConfig& Runtime::getConfig() const {
        return m_config;
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

    v8::Local<v8::Value> Runtime::executeString(const String& code, const String& filename) {
        return m_scriptSystem->executeString(code, filename);
    }

    void Runtime::commitBindings() {
        m_bindingModule->commitBindings();
        m_scriptSystem->onAfterBindings();
    }

    bool Runtime::buildProject() {
        logDebug("Compiling project");
        if (!m_typeScriptCompilerModule->compileDirectory(m_config.scriptRootDirectory)) {
            logError("Failed to compile project");
            return false;
        }

        return true;
    }

    void Runtime::submitJob(IJob* job) {
        m_threadPool.submitJob(job);
    }

    void Runtime::submitJobs(const Array<IJob*>& jobs) {
        m_threadPool.submitJobs(jobs);
    }

    bool Runtime::service() {
        v8::Isolate* isolate = m_scriptSystem->getIsolate();
        v8::Isolate::Scope isolateScope(isolate);
        v8::HandleScope hs(isolate);
        v8::Local<v8::Context> context = m_scriptSystem->getContext();
        v8::Context::Scope contextScope(context);

        bool didHaveWork = m_threadPool.processCompleted();
        isolate->PerformMicrotaskCheckpoint();

        return didHaveWork;
    }
}
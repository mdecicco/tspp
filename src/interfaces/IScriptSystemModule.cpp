#include <tspp/interfaces/IScriptSystemModule.h>
#include <tspp/systems/script.h>

namespace tspp {
    IScriptSystemModule::IScriptSystemModule(
        ScriptSystem* scriptSystem,
        const char* logContextName,
        const char* submoduleName
    )
        : IWithLogging(logContextName), m_scriptSystem(scriptSystem), m_name(submoduleName) {
    }
    
    IScriptSystemModule::~IScriptSystemModule() {
    }

    bool IScriptSystemModule::initialize() {
        return true;
    }

    bool IScriptSystemModule::onAfterBindings() {
        return true;
    }
    
    void IScriptSystemModule::shutdown() {
    }
    
    ScriptSystem* IScriptSystemModule::getScriptSystem() const {
        return m_scriptSystem;
    }

    const char* IScriptSystemModule::getName() const {
        return m_name;
    }
} 
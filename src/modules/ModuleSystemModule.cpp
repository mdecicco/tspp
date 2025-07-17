#include <tspp/modules/ModuleSystemModule.h>
#include <tspp/systems/script.h>
#include <utils/Exception.h>
#include <utils/Array.hpp>

namespace tspp {
    ModuleSystemModule::ModuleEntry& ModuleSystemModule::ModuleEntry::operator=(const ModuleEntry& other) {
        isolate = other.isolate;
        state = other.state;
        exports.Reset(isolate, other.exports);
        factory.Reset(isolate, other.factory);
        dependencies = other.dependencies;
        return *this;
    }
    

    void DefineCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);
        ModuleSystemModule* moduleSystem = (ModuleSystemModule*)args.Data().As<v8::External>()->Value();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        // Parse arguments based on AMD spec
        String id;
        Array<String> dependencies;
        v8::Local<v8::Function> factory;

        // Check argument count and types
        if (args.Length() < 1) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "define requires at least a factory function").ToLocalChecked()));
            return;
        }
        
        int argIndex = 0;
        
        // First argument might be the module ID (string)
        if (args.Length() > 1 && args[0]->IsString()) {
            v8::String::Utf8Value idStr(isolate, args[0]);
            id = *idStr;
            argIndex++;
        }
        
        // Next argument might be dependencies (array)
        if (args.Length() > argIndex && args[argIndex]->IsArray()) {
            v8::Local<v8::Array> deps = v8::Local<v8::Array>::Cast(args[argIndex]);

            for (uint32_t i = 0; i < deps->Length(); i++) {
                v8::Local<v8::Value> dep;
                if (deps->Get(context, i).ToLocal(&dep) && dep->IsString()) {
                    v8::String::Utf8Value depStr(isolate, dep);
                    dependencies.push(*depStr);
                } else {
                    isolate->ThrowException(v8::Exception::TypeError(
                        v8::String::NewFromUtf8(isolate, "define requires an array of strings as dependencies").ToLocalChecked()));
                    return;
                }
            }

            argIndex++;
        }
        
        // Last argument must be the factory function
        if (argIndex < args.Length() && args[argIndex]->IsFunction()) {
            factory = v8::Local<v8::Function>::Cast(args[argIndex]);
        } else {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "define requires a factory function").ToLocalChecked()));
            return;
        }
        
        // Define the module
        bool success = moduleSystem->defineModule(id, dependencies, factory);
        
        if (!success) {
            isolate->ThrowException(v8::Exception::Error(
                v8::String::NewFromUtf8(isolate, "Failed to define module").ToLocalChecked()));
            return;
        }
    }
    
    void RequireCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);
        ModuleSystemModule* moduleSystem = (ModuleSystemModule*)args.Data().As<v8::External>()->Value();
        
        // Check arguments
        if (args.Length() < 1 || !args[0]->IsString()) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "require requires a module ID string").ToLocalChecked()));
            return;
        }
        
        // Get the module ID
        v8::String::Utf8Value idStr(isolate, args[0]);
        String id = *idStr;
        
        // Require the module
        v8::Local<v8::Value> exports = moduleSystem->requireModule(id);
        
        // Return the module exports
        args.GetReturnValue().Set(exports);
    }
    
    void RelativeRequireCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        v8::Local<v8::Object> data = args.Data().As<v8::Object>();
        v8::Local<v8::Value> moduleSystemValue = data->Get(context, v8::String::NewFromUtf8(isolate, "a").ToLocalChecked()).ToLocalChecked();
        v8::Local<v8::Value> moduleObjValue = data->Get(context, v8::String::NewFromUtf8(isolate, "b").ToLocalChecked()).ToLocalChecked();
        
        v8::Local<v8::Object> moduleObj = moduleObjValue.As<v8::Object>();
        ModuleSystemModule* moduleSystem = (ModuleSystemModule*)moduleSystemValue.As<v8::External>()->Value();

        // Check arguments
        if (args.Length() < 1 || !args[0]->IsString()) {
            isolate->ThrowException(
                v8::Exception::TypeError(
                    v8::String::NewFromUtf8(isolate, "require requires a module ID string").ToLocalChecked()
                )
            );
            return;
        }
        
        // Get the module ID
        v8::String::Utf8Value idStr(isolate, args[0]);
        String id = *idStr;
        
        // Get the parent module ID
        v8::MaybeLocal<v8::Value> thisId = moduleObj->Get(context, v8::String::NewFromUtf8(isolate, "id").ToLocalChecked());
        v8::Local<v8::Value> moduleValue;
        String parentId;
        
        if (thisId.ToLocal(&moduleValue) && moduleValue->IsString()) {
            v8::String::Utf8Value parentIdStr(isolate, moduleValue);
            parentId = *parentIdStr;
        }
        
        // Resolve the module ID relative to the parent
        String resolvedId = moduleSystem->resolveModuleId(id, parentId);
        
        // Require the module
        v8::Local<v8::Value> exports = moduleSystem->requireModule(resolvedId);
        
        // Return the module exports
        args.GetReturnValue().Set(exports);
    }


    ModuleSystemModule::ModuleSystemModule(ScriptSystem* scriptSystem, Runtime* runtime)
        : IScriptSystemModule(scriptSystem, "ModuleSystem", "ModuleSystem")
    {
        m_runtime = runtime;
    }
    
    ModuleSystemModule::~ModuleSystemModule() {
        shutdown();
    }
    
    bool ModuleSystemModule::initialize() {
        // Set up the global define() and require() functions
        setupDefineFunction();
        setupRequireFunction();

        return true;
    }
    
    void ModuleSystemModule::shutdown() {
        // Clear module registry
        m_modules.clear();
        
        // Clear global function references
        m_defineFunc.Reset();
        m_requireFunc.Reset();
    }
    
    void ModuleSystemModule::setupDefineFunction() {
        v8::Isolate* isolate = m_scriptSystem->getIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = m_scriptSystem->getContext();
        v8::Context::Scope contextScope(context);
        
        // Create the define function
        v8::Local<v8::Function> defineFunc = v8::Function::New(
            context,
            DefineCallback,
            v8::External::New(isolate, this)
        ).ToLocalChecked();
        
        // Add the define.amd property (for AMD compliance)
        defineFunc->Set(
            context,
            v8::String::NewFromUtf8(isolate, "amd").ToLocalChecked(),
            v8::Object::New(isolate)
        ).Check();
        
        // Store the function
        m_defineFunc.Reset(isolate, defineFunc);
        
        // Add it to the global object
        v8::Local<v8::Object> global = context->Global();
        global->Set(
            context,
            v8::String::NewFromUtf8(isolate, "define").ToLocalChecked(),
            defineFunc
        ).Check();
    }
    
    void ModuleSystemModule::setupRequireFunction() {
        v8::Isolate* isolate = m_scriptSystem->getIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = m_scriptSystem->getContext();
        v8::Context::Scope contextScope(context);
        
        // Create the require function
        v8::Local<v8::Function> requireFunc = v8::Function::New(
            context,
            RequireCallback,
            v8::External::New(isolate, this)
        ).ToLocalChecked();
        
        // Store the function
        m_requireFunc.Reset(isolate, requireFunc);
        
        // Add it to the global object
        v8::Local<v8::Object> global = context->Global();
        global->Set(
            context,
            v8::String::NewFromUtf8(isolate, "require").ToLocalChecked(),
            requireFunc
        ).Check();
    }
    
    bool ModuleSystemModule::registerBuiltInModule(const String& id, v8::Local<v8::Object> moduleObject) {
        v8::Isolate* isolate = m_scriptSystem->getIsolate();
        v8::HandleScope scope(isolate);
        
        // Check if module already exists
        if (m_modules.find(id) != m_modules.end()) {
            logWarn("Module '%s' is already registered", id.c_str());
            return false;
        }
        
        // Create a new module entry
        ModuleEntry entry;
        entry.isolate = isolate;
        entry.state = ModuleEntry::State::Loaded; // Built-in modules are already loaded
        entry.exports.Reset(isolate, moduleObject);
        
        // Add to registry
        m_modules[id] = entry;
        
        logDebug("Registered built-in module: %s", id.c_str());
        return true;
    }
    
    bool ModuleSystemModule::defineModule(
        const String& id, 
        const Array<String>& deps, 
        v8::Local<v8::Function> factory
    ) {
        v8::Isolate* isolate = m_scriptSystem->getIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = m_scriptSystem->getContext();
        
        // Generate a unique ID for anonymous modules if needed
        String moduleId = id;
        if (moduleId.size() == 0) {
            // For now, we'll just use a timestamp-based ID
            // In a real implementation, this would be based on the script URL
            moduleId = String::Format("anonymous_%lld", (long long)time(nullptr));
        }
        
        // Check if module already exists
        if (m_modules.find(moduleId) != m_modules.end()) {
            logWarn("Module '%s' is already defined", moduleId.c_str());
            return false;
        }
        
        // Create a new module entry
        ModuleEntry entry;
        entry.isolate = isolate;
        entry.state = ModuleEntry::State::Registered;
        entry.factory.Reset(isolate, factory);
        entry.dependencies = deps;
        
        // Add to registry
        m_modules[moduleId] = entry;
        
        logDebug("Defined module: %s", moduleId.c_str());
        return true;
    }
    
    v8::Local<v8::Value> ModuleSystemModule::requireModule(const String& id) {
        v8::Isolate* isolate = m_scriptSystem->getIsolate();
        v8::Isolate::Scope isolateScope(isolate);

        v8::EscapableHandleScope scope(isolate);
        
        // Resolve the module ID
        String resolvedId = resolveModuleId(id);
        
        // Load the module
        v8::Local<v8::Value> exports = loadModule(resolvedId);
        
        return scope.Escape(exports);
    }
    
    String ModuleSystemModule::resolveModuleId(const String& id, const String& baseId) {
        // For now, we'll just return the ID as-is
        // In a real implementation, this would handle relative paths
        return id;
    }
    
    v8::Local<v8::Value> ModuleSystemModule::loadModule(const String& id) {
        v8::Isolate* isolate = m_scriptSystem->getIsolate();
        v8::Isolate::Scope isolateScope(isolate);

        v8::EscapableHandleScope scope(isolate);
        v8::Local<v8::Context> context = m_scriptSystem->getContext();
        v8::Context::Scope contextScope(context);
        
        // Check if module exists
        auto it = m_modules.find(id);
        if (it == m_modules.end()) {
            // Module not found
            logError("Module '%s' not found", id.c_str());
            return v8::Undefined(isolate);
        }
        
        ModuleEntry& entry = it->second;
        
        // If module is already loaded, return its exports
        if (entry.state == ModuleEntry::State::Loaded) {
            return scope.Escape(entry.exports.Get(isolate));
        }
        
        // Check for circular dependencies
        if (entry.state == ModuleEntry::State::Loading) {
            logError("Circular dependency detected for module '%s'", id.c_str());
            return v8::Undefined(isolate);
        }
        
        // Mark module as loading
        entry.state = ModuleEntry::State::Loading;
        
        // Create module, exports, and require objects
        v8::Local<v8::Object> moduleObj = v8::Object::New(isolate);
        v8::Local<v8::Object> exportsObj = v8::Object::New(isolate);
        v8::Local<v8::Function> requireFunc;
        
        createModuleArgs(id, moduleObj, exportsObj, requireFunc);
        
        // Set exports property on module object
        moduleObj->Set(
            context,
            v8::String::NewFromUtf8(isolate, "exports").ToLocalChecked(),
            exportsObj
        ).Check();
        
        // Load dependencies
        Array<v8::Local<v8::Value>> args;
        
        for (const String& depId : entry.dependencies) {
            if (depId == "require") {
                args.push(requireFunc);
            } else if (depId == "exports") {
                args.push(exportsObj);
            } else if (depId == "module") {
                args.push(moduleObj);
            } else {
                // Load the dependency
                v8::Local<v8::Value> depExports = loadModule(resolveModuleId(depId, id));
                args.push(depExports);
            }
        }
        
        // Call the factory function
        v8::Local<v8::Function> factory = entry.factory.Get(isolate);
        v8::Local<v8::Value> result;
        v8::TryCatch tryCatch(isolate);
        
        v8::Local<v8::Value>* argArray = nullptr;
        if (args.size() > 0) {
            argArray = new v8::Local<v8::Value>[args.size()];
            for (size_t i = 0; i < args.size(); i++) {
                argArray[i] = args[i];
            }
        }
        
        bool success = factory->Call(
            context,
            v8::Undefined(isolate),
            args.size(),
            argArray
        ).ToLocal(&result);
        
        if (argArray) {
            delete[] argArray;
        }
        
        if (!success) {
            logError("Error executing factory function for module '%s'", id.c_str());
            if (tryCatch.HasCaught()) {
                v8::String::Utf8Value error(isolate, tryCatch.Exception());
                logError("  %s", *error);
            }
            return v8::Undefined(isolate);
        }
        
        // If the factory returned a value and it's not undefined, use it as the exports
        if (!result->IsUndefined()) {
            exportsObj = v8::Local<v8::Object>::Cast(result);
        } else {
            // Otherwise, use the exports object that was passed to the factory
            exportsObj = v8::Local<v8::Object>::Cast(
                moduleObj->Get(
                    context,
                    v8::String::NewFromUtf8(isolate, "exports").ToLocalChecked()
                ).ToLocalChecked()
            );
        }
        
        // Store the exports and mark as loaded
        entry.exports.Reset(isolate, exportsObj);
        entry.state = ModuleEntry::State::Loaded;
        
        return scope.Escape(exportsObj);
    }
    
    void ModuleSystemModule::createModuleArgs(
        const String& id, 
        v8::Local<v8::Object>& moduleObj, 
        v8::Local<v8::Object>& exportsObj, 
        v8::Local<v8::Function>& requireFunc
    ) {
        v8::Isolate* isolate = m_scriptSystem->getIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = m_scriptSystem->getContext();
        
        // Set id property on module object
        moduleObj->Set(
            context,
            v8::String::NewFromUtf8(isolate, "id").ToLocalChecked(),
            v8::String::NewFromUtf8(isolate, id.c_str()).ToLocalChecked()
        ).Check();

        v8::Local<v8::Object> data = v8::Object::New(isolate);
        data->Set(context, v8::String::NewFromUtf8(isolate, "a").ToLocalChecked(), v8::External::New(isolate, this));
        data->Set(context, v8::String::NewFromUtf8(isolate, "b").ToLocalChecked(), moduleObj);
        
        // Create a local require function that resolves relative to this module
        requireFunc = v8::Function::New(
            context,
            RelativeRequireCallback,
            data
        ).ToLocalChecked();
    }
} 
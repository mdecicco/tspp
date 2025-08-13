#include <tspp/builtin/compiler.h>
#include <tspp/builtin/tsc.h>
#include <tspp/modules/TypeScriptCompilerModule.h>
#include <tspp/tspp.h>

#include <stdio.h>

namespace tspp {
    TypeScriptCompilerModule::TypeScriptCompilerModule(ScriptSystem* scriptSystem, Runtime* runtime)
        : IScriptSystemModule(scriptSystem, "TypeScriptCompiler", "TypeScript Compiler") {
        m_runtime = runtime;
    }

    TypeScriptCompilerModule::~TypeScriptCompilerModule() {
        shutdown();
    }

    bool TypeScriptCompilerModule::initialize() {
        // Load the TypeScript compiler
        if (!loadCompiler()) {
            error("Failed to load TypeScript compiler");
            return false;
        }

        // Load the compilation functions
        if (!loadCompilationShims()) {
            error("Failed to load TypeScript compilation shims");
            return false;
        }

        debug("TypeScript compiler initialized (version %s)", m_version.c_str());
        return true;
    }

    bool TypeScriptCompilerModule::onAfterBindings() {
        v8::Isolate* isolate = m_runtime->getIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = m_runtime->getContext();

        v8::Local<v8::Function> getter = m_compileFuncFactory.Get(isolate);
        m_compileFuncFactory.Reset();

        v8::Local<v8::Value> factoryResultVal;
        if (!getter->Call(context, v8::Null(isolate), 0, nullptr).ToLocal(&factoryResultVal) ||
            !factoryResultVal->IsObject()) {
            error("Shim factory function returned invalid result");
            return false;
        }

        v8::Local<v8::Object> factoryResult = factoryResultVal.As<v8::Object>();

        v8::Local<v8::Value> compileFile;
        factoryResult->Get(context, v8::String::NewFromUtf8(isolate, "compileFile").ToLocalChecked())
            .ToLocal(&compileFile);

        if (compileFile.IsEmpty() || !compileFile->IsFunction()) {
            error("compileFile function not found");
            return false;
        }

        m_compileFile.Reset(isolate, compileFile.As<v8::Function>());

        v8::Local<v8::Value> compileDirectory;
        factoryResult->Get(context, v8::String::NewFromUtf8(isolate, "compileDirectory").ToLocalChecked())
            .ToLocal(&compileDirectory);

        if (compileDirectory.IsEmpty() || !compileDirectory->IsFunction()) {
            error("compileDirectory function not found");
            return false;
        }

        m_compileDirectory.Reset(isolate, compileDirectory.As<v8::Function>());

        return true;
    }

    void TypeScriptCompilerModule::shutdown() {
        m_tsCompiler.Reset();
        m_compileFile.Reset();
        m_compileDirectory.Reset();
        m_compileFuncFactory.Reset();
    }

    bool TypeScriptCompilerModule::loadCompiler() {
        try {
            // Read the TypeScript compiler code
            String compilerCode;
            compilerCode.copy((const char*)tsc_code, tsc_code_len);

            // Execute the TypeScript compiler code
            v8::Isolate* isolate = m_runtime->getIsolate();
            v8::HandleScope scope(isolate);
            v8::Local<v8::Context> context = m_runtime->getContext();

            v8::Local<v8::Value> result = m_runtime->executeString(compilerCode, "tsc.js");
            if (result.IsEmpty()) {
                error("Failed to execute TypeScript compiler code");
                return false;
            }

            // Get the TypeScript compiler object
            v8::Local<v8::Object> global = context->Global();
            v8::Local<v8::Value> ts;
            if (!global->Get(context, v8::String::NewFromUtf8(isolate, "ts").ToLocalChecked()).ToLocal(&ts) ||
                !ts->IsObject()) {
                error("TypeScript compiler not found in global scope");
                return false;
            }

            // Store the TypeScript compiler object
            m_tsCompiler.Reset(isolate, ts.As<v8::Object>());

            // Get the TypeScript compiler version
            v8::MaybeLocal<v8::Value> maybeVersion =
                ts.As<v8::Object>()->Get(context, v8::String::NewFromUtf8(isolate, "version").ToLocalChecked());

            if (maybeVersion.IsEmpty()) {
                error("TypesScript compiler object does not have a version property");
                return false;
            }

            v8::Local<v8::Value> version;
            if (!maybeVersion.ToLocal(&version)) {
                error("Failed to get TypeScript compiler version");
                return false;
            }

            v8::String::Utf8Value versionStr(isolate, version);
            m_version = *versionStr;

            return true;
        } catch (const GenericException& e) {
            error("Exception while loading TypeScript compiler: %s", e.what());
            return false;
        }
    }

    bool TypeScriptCompilerModule::loadCompilationShims() {
        String code;
        code.copy((const char*)compiler_code, compiler_code_len);

        v8::Isolate* isolate = m_runtime->getIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = m_runtime->getContext();

        v8::Local<v8::Value> result = m_runtime->executeString(code, "compiler.js");
        if (result.IsEmpty()) {
            error("Failed to execute compilation shim script");
            return false;
        }

        if (!result->IsFunction()) {
            error("Compilation shims factory not found");
            return false;
        }

        m_compileFuncFactory.Reset(isolate, result.As<v8::Function>());
        return true;
    }

    bool TypeScriptCompilerModule::compile(const String& fileName) {
        log("Compiling TypeScript file %s", fileName.c_str());
        v8::Isolate* isolate = m_runtime->getIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = m_runtime->getContext();

        try {
            v8::Local<v8::Function> compileFile = m_compileFile.Get(isolate);

            v8::Local<v8::Value> args[] = {v8::String::NewFromUtf8(isolate, fileName.c_str()).ToLocalChecked()};

            std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
            v8::MaybeLocal<v8::Value> maybeResult              = compileFile->Call(context, v8::Null(isolate), 1, args);
            std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
            u32 duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            v8::Local<v8::Value> result;
            if (!maybeResult.ToLocal(&result) || !result->IsBoolean()) {
                error("Compilation failed with invalid result after %dms", duration);
                return false;
            }

            if (result->IsTrue()) {
                log("Compilation succeeded after %dms", duration);
                return true;
            }

            error("Compilation failed after %dms", duration);
            return false;
        } catch (const GenericException& e) {
            error(e.what());
            return false;
        }
    }

    bool TypeScriptCompilerModule::compileDirectory(const String& path) {
        debug("Compiling TypeScript project in %s", path.c_str());
        v8::Isolate* isolate = m_runtime->getIsolate();
        v8::Isolate::Scope isolateScope(isolate);
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = m_runtime->getContext();

        try {
            v8::Local<v8::Function> compileDirectory = m_compileDirectory.Get(isolate);

            v8::Local<v8::Value> args[] = {v8::String::NewFromUtf8(isolate, path.c_str()).ToLocalChecked()};

            std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
            v8::MaybeLocal<v8::Value> maybeResult = compileDirectory->Call(context, v8::Null(isolate), 1, args);
            std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
            u32 duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            v8::Local<v8::Value> result;
            if (!maybeResult.ToLocal(&result) || !result->IsBoolean()) {
                error("Compilation failed with invalid result after %dms", duration);
                return false;
            }

            if (result->IsTrue()) {
                debug("Compilation succeeded after %dms", duration);
                return true;
            }

            error("Compilation failed after %dms", duration);
            return false;
        } catch (const GenericException& e) {
            error(e.what());
            return false;
        }
    }

    String TypeScriptCompilerModule::getVersion() {
        return m_version;
    }
}
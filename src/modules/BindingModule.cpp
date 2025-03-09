#include <tspp/modules/BindingModule.h>
#include <tspp/systems/script.h>
#include <tspp/utils/PrimitiveMarshaller.h>
#include <tspp/utils/PointerMarshaller.h>
#include <tspp/utils/UtilsStringMarshaller.h>
#include <tspp/utils/TrivialStructMarshaller.h>
#include <tspp/utils/NonTrivialMarshaller.h>
#include <tspp/utils/ArrayMarshaller.h>
#include <tspp/utils/CallContext.h>
#include <tspp/utils/BindObjectType.h>
#include <tspp/utils/CallProxy.h>
#include <tspp/utils/JavaScriptTypeData.h>
#include <tspp/utils/HostObjectManager.h>
#include <tspp/tspp.h>

namespace tspp {
    BindingModule::BindingModule(ScriptSystem* scriptSystem, Runtime* runtime)
        : IScriptSystemModule(scriptSystem, "Binding", "Binding")
    {
        m_runtime = runtime;
    }
    
    BindingModule::~BindingModule() {
        shutdown();
    }
    
    bool BindingModule::initialize() {
        try {
            bind::Registry::GlobalNamespace();
        } catch (Exception& e) {
            logError("API binding registry not initialized");
            return false;
        }

        bindBuiltInTypes();

        return true;
    }
    
    void BindingModule::shutdown() {
    }

    void BindingModule::commitBindings() {
        bind::Namespace* global = nullptr;
        
        try {
            global = bind::Registry::GlobalNamespace();
        } catch (Exception& e) {
            logError("Failed to get global namespace: %s", e.what());
            return;
        }

        v8::Isolate* isolate = m_runtime->getIsolate();
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope scope(isolate);

        v8::Local<v8::Context> context = m_runtime->getContext();
        v8::Context::Scope context_scope(context);

        // Set up data type user data
        const Array<bind::DataType*>& dataTypes = bind::Registry::Types();
        for (bind::DataType* dataType : dataTypes) {
            const bind::type_meta& meta = dataType->getInfo();

            DataTypeUserData& userData = dataType->getUserData<DataTypeUserData>();
            if (userData.marshallingData) {
                // Was set up manually
                continue;
            }

            if (userData.arrayElementType) {
                userData.marshallingData = new ArrayMarshaller(dataType);
                continue;
            }

            if (meta.is_primitive) {
                userData.marshallingData = new PrimitiveMarshaller(dataType);
            } else if (meta.is_pointer) {
                userData.marshallingData = new PointerMarshaller(dataType);
            } else if (meta.is_trivially_constructible) {
                userData.marshallingData = new TrivialStructMarshaller(dataType);
            } else {
                userData.marshallingData = new NonTrivialMarshaller(dataType);
            }
        }

        // Get all global symbols
        const Array<bind::ISymbol*>& symbols = global->getSymbols();
        for (bind::ISymbol* symbol : symbols) {
            processGlobalSymbol(symbol);
        }
    }

    void BindingModule::bindBuiltInTypes() {
        bind::type<void>("void");
        bind::type<bool>("bool");
        bind::type<i8>("i8");
        bind::type<i16>("i16");
        bind::type<i32>("i32");
        bind::type<i64>("i64");
        bind::type<u8>("u8");
        bind::type<u16>("u16");
        bind::type<u32>("u32");
        bind::type<u64>("u64");
        bind::type<f32>("f32");
        bind::type<f64>("f64");
        
        bind::ObjectTypeBuilder<String> str = bind::type<String>("String");
        str.dtor();

        DataTypeUserData& userData = str.getType()->getUserData<DataTypeUserData>();
        userData.typescriptType = "string";
        userData.marshallingData = new UtilsStringMarshaller(str.getType());
    }

    void BindingModule::processGlobalSymbol(bind::ISymbol* symbol) {
        switch (symbol->getSymbolType()) {
            case bind::SymbolType::Namespace:
                processNamespace((bind::Namespace*)symbol);
                break;
            default:
                break;
        }
    }

    void BindingModule::processNamespace(bind::Namespace* ns) {
        logDebug("Binding global namespace '%s' as module", ns->getName().c_str());

        v8::Isolate* isolate = m_runtime->getIsolate();
        v8::Local<v8::Context> context = m_runtime->getContext();
        v8::Local<v8::Object> exports = v8::Object::New(isolate);

        const Array<bind::ISymbol*>& symbols = ns->getSymbols();
        for (bind::ISymbol* symbol : symbols) {
            switch (symbol->getSymbolType()) {
                case bind::SymbolType::Function:
                    exports->Set(
                        context,
                        v8::String::NewFromUtf8(isolate, symbol->getName().c_str()).ToLocalChecked(),
                        processFunction((bind::Function*)symbol)
                    ).Check();
                    break;
                case bind::SymbolType::DataType: {
                    bind::DataType* dataType = (bind::DataType*)symbol;
                    v8::Local<v8::Value> val = processDataType(dataType);
                    if (!val.IsEmpty()) {
                        exports->Set(
                            context,
                            v8::String::NewFromUtf8(isolate, symbol->getName().c_str()).ToLocalChecked(),
                            val
                        ).Check();
                    }
                    break;
                }
                default:
                    break;
            }
        }

        m_runtime->registerBuiltInModule(ns->getName(), exports);
    }

    v8::Local<v8::Function> BindingModule::processFunction(bind::Function* function) {
        v8::Isolate* isolate = m_runtime->getIsolate();
        v8::Local<v8::Context> context = m_runtime->getContext();

        return v8::Function::New(
            context,
            FunctionCallProxy,
            v8::External::New(isolate, function),
            0,
            v8::ConstructorBehavior::kThrow
        ).ToLocalChecked();
    }

    v8::Local<v8::Value> BindingModule::processDataType(bind::DataType* dataType) {
        v8::Isolate* isolate = m_runtime->getIsolate();
        v8::Local<v8::Context> context = m_runtime->getContext();

        DataTypeUserData& userData = dataType->getUserData<DataTypeUserData>();

        const bind::type_meta& meta = dataType->getInfo();
        if (meta.is_enum) {
            v8::Local<v8::Object> obj = v8::Object::New(isolate);

            bind::EnumType* enumType = (bind::EnumType*)dataType;
            const Array<bind::EnumType::Field>& fields = enumType->getFields();
            for (const bind::EnumType::Field& field : fields) {
                if (meta.is_unsigned) {
                    obj->Set(
                        context,
                        v8::String::NewFromUtf8(isolate, field.name.c_str()).ToLocalChecked(),
                        v8::Number::New(isolate, (f64)field.value.u)
                    ).Check();
                } else {
                    obj->Set(
                        context,
                        v8::String::NewFromUtf8(isolate, field.name.c_str()).ToLocalChecked(),
                        v8::Number::New(isolate, (f64)field.value.i)
                    ).Check();
                }
            }

            return obj;
        }
        
        if (meta.is_trivially_constructible == 0) {
            v8::Local<v8::FunctionTemplate> ctor = buildPrototype(isolate, dataType);
            userData.javascriptData = new JavaScriptTypeData();
            userData.javascriptData->constructor.Reset(isolate, ctor);
            userData.hostObjectManager = new HostObjectManager(dataType);
            userData.hostObjectManager->addLogHandler(this);

            return ctor->GetFunction(context).ToLocalChecked();
        }

        return v8::Local<v8::Value>();
    }
} 
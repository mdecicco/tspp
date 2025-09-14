#include <tspp/builtin/core.d.h>
#include <tspp/builtin/databuffer.h>
#include <tspp/builtin/process.h>
#include <tspp/marshalling/ArrayMarshaller.h>
#include <tspp/marshalling/FunctionMarshaller.h>
#include <tspp/marshalling/NonTrivialMarshaller.h>
#include <tspp/marshalling/PointerMarshaller.h>
#include <tspp/marshalling/PrimitiveMarshaller.h>
#include <tspp/marshalling/TrivialStructMarshaller.h>
#include <tspp/marshalling/UtilsStringMarshaller.h>
#include <tspp/modules/BindingModule.h>
#include <tspp/systems/script.h>
#include <tspp/tspp.h>
#include <tspp/utils/BindObjectType.h>
#include <tspp/utils/CallContext.h>
#include <tspp/utils/CallProxy.h>
#include <tspp/utils/Docs.h>
#include <tspp/utils/HostObjectManager.h>
#include <tspp/utils/JavaScriptTypeData.h>
#include <tspp/utils/SourceFileBuilder.h>

#include <filesystem>

namespace tspp {
    BindingModule::BindingModule(ScriptSystem* scriptSystem, Runtime* runtime)
        : IScriptSystemModule(scriptSystem, "Binding", "Binding") {
        m_runtime = runtime;
    }

    BindingModule::~BindingModule() {
        shutdown();
    }

    bool BindingModule::initialize() {
        try {
            bind::Registry::GlobalNamespace();
        } catch (GenericException& e) {
            error("API binding registry not initialized");
            return false;
        }

        bindBuiltInTypes();

        return true;
    }

    void BindingModule::shutdown() {}

    void BindingModule::commitBindings() {
        bind::Namespace* global = nullptr;

        try {
            global = bind::Registry::GlobalNamespace();
        } catch (GenericException& e) {
            error("Failed to get global namespace: %s", e.what());
            return;
        }

        SourceFileBuilder dts;
        dts.line("interface IntrinsicNumber<");
        dts.indent();
        dts.line("bits extends number,");
        dts.line("is_signed extends boolean,");
        dts.line("is_floating_point extends boolean");
        dts.unindent();
        dts.line("> extends Number {}");
        dts.line("/** %d */", INT8_MIN);
        dts.line("declare const I8_MIN: number;");
        dts.line("/** %d */", INT8_MAX);
        dts.line("declare const I8_MAX: number;");
        dts.line("/** %d */", INT16_MIN);
        dts.line("declare const I16_MIN: number;");
        dts.line("/** %d */", INT16_MAX);
        dts.line("declare const I16_MAX: number;");
        dts.line("/** %d */", INT32_MIN);
        dts.line("declare const I32_MIN: number;");
        dts.line("/** %d */", INT32_MAX);
        dts.line("declare const I32_MAX: number;");
        dts.line("/** %lld */", INT64_MIN);
        dts.line("declare const I64_MIN: number;");
        dts.line("/** %lld */", INT64_MAX);
        dts.line("declare const I64_MAX: number;");
        dts.line("/** 0 */");
        dts.line("declare const U8_MIN: number;");
        dts.line("/** %d */", UINT8_MAX);
        dts.line("declare const U8_MAX: number;");
        dts.line("/** 0 */");
        dts.line("declare const U16_MIN: number;");
        dts.line("/** %d */", UINT16_MAX);
        dts.line("declare const U16_MAX: number;");
        dts.line("/** 0 */");
        dts.line("declare const U32_MIN: number;");
        dts.line("/** %d */", UINT32_MAX);
        dts.line("declare const U32_MAX: number;");
        dts.line("/** 0 */");
        dts.line("declare const U64_MIN: number;");
        dts.line("/** %llu */", UINT64_MAX);
        dts.line("declare const U64_MAX: number;");
        dts.line("/** 1.175494351e-38 */");
        dts.line("declare const F32_MIN: number;");
        dts.line("/** 3.402823466e+38 */");
        dts.line("declare const F32_MAX: number;");
        dts.line("/** 2.2250738585072014e-308 */");
        dts.line("declare const F64_MIN: number;");
        dts.line("/** 1.7976931348623158e+308 */");
        dts.line("declare const F64_MAX: number;");

        dts.line("declare function setTimeout(callback: () => void, delay?: number): number;");
        dts.line(
            "declare function setTimeout<Args extends any[]>(callback: (...args: Args) => void, delay: number, "
            "...args: Args): number;"
        );
        dts.line("declare function setInterval(callback: () => void, delay?: number): number;");
        dts.line(
            "declare function setInterval<Args extends any[]>(callback: (...args: Args) => void, delay: number, "
            "...args: Args): number;"
        );
        dts.line("declare function clearInterval(id: number): void;");
        dts.line("declare function clearTimeout(id: number): void;");

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
            if (userData.marshaller) {
                // Was set up manually
                continue;
            }

            if (userData.arrayElementType) {
                userData.marshaller = new ArrayMarshaller(dataType);
                continue;
            }

            if (meta.is_primitive || meta.is_opaque) {
                userData.marshaller = new PrimitiveMarshaller(dataType);
            } else if (meta.is_function) {
                userData.marshaller = new FunctionMarshaller(dataType);
            } else if (meta.is_pointer) {
                userData.marshaller = new PointerMarshaller(dataType);
            } else if (meta.is_trivially_constructible) {
                userData.marshaller = new TrivialStructMarshaller(dataType);
            } else {
                userData.marshaller = new NonTrivialMarshaller(dataType);
            }
        }

        // Get all global symbols
        const Array<bind::ISymbol*>& symbols = global->getSymbols();
        for (bind::ISymbol* symbol : symbols) {
            processGlobalSymbol(dts, symbol);
        }

        v8::Local<v8::Object> globalScope = context->Global();

        /* clang-format off */
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "I8_MIN" ).ToLocalChecked(), v8::Number::New(isolate, INT8_MIN  )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "I8_MAX" ).ToLocalChecked(), v8::Number::New(isolate, INT8_MAX  )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "I16_MIN").ToLocalChecked(), v8::Number::New(isolate, INT16_MIN )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "I16_MAX").ToLocalChecked(), v8::Number::New(isolate, INT16_MAX )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "I32_MIN").ToLocalChecked(), v8::Number::New(isolate, INT32_MIN )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "I32_MAX").ToLocalChecked(), v8::Number::New(isolate, INT32_MAX )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "I64_MIN").ToLocalChecked(), v8::Number::New(isolate, INT64_MIN )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "I64_MAX").ToLocalChecked(), v8::Number::New(isolate, INT64_MAX )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "U8_MIN ").ToLocalChecked(), v8::Number::New(isolate, 0         )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "U8_MAX" ).ToLocalChecked(), v8::Number::New(isolate, UINT8_MAX )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "U16_MIN").ToLocalChecked(), v8::Number::New(isolate, 0         )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "U16_MAX").ToLocalChecked(), v8::Number::New(isolate, UINT16_MAX)).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "U32_MIN").ToLocalChecked(), v8::Number::New(isolate, 0         )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "U32_MAX").ToLocalChecked(), v8::Number::New(isolate, UINT32_MAX)).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "U64_MIN").ToLocalChecked(), v8::Number::New(isolate, 0         )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "U64_MAX").ToLocalChecked(), v8::Number::New(isolate, UINT64_MAX)).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "F32_MIN").ToLocalChecked(), v8::Number::New(isolate, FLT_MIN   )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "F32_MAX").ToLocalChecked(), v8::Number::New(isolate, FLT_MAX   )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "F64_MIN").ToLocalChecked(), v8::Number::New(isolate, DBL_MIN   )).Check();
        globalScope->Set(context, v8::String::NewFromUtf8(isolate, "F64_MAX").ToLocalChecked(), v8::Number::New(isolate, DBL_MAX   )).Check();
        /* clang-format on */

        std::filesystem::path root        = m_runtime->getConfig().scriptRootDirectory;
        std::filesystem::path builtinDefs = root / "internal" / "lib" / "builtins.d.ts";

        if (!std::filesystem::exists(builtinDefs.parent_path())) {
            if (!std::filesystem::create_directories(builtinDefs.parent_path())) {
                error("Failed to create internal/lib directory in '%s'", root.string().c_str());
                return;
            }
        }

        std::filesystem::path coreDefs = root / "internal" / "lib" / "core.d.ts";
        if (!std::filesystem::exists(coreDefs)) {
            FILE* file = fopen(coreDefs.string().c_str(), "w");
            if (!file) {
                error("Failed to create core definitions file in '%s'", coreDefs.string().c_str());
                return;
            }

            if (fwrite(core_code, 1, core_code_len, file) != core_code_len) {
                error("Failed to write core definitions to '%s'", coreDefs.string().c_str());
                fclose(file);
                return;
            }

            fclose(file);
        }

        if (!dts.writeToFile(builtinDefs.string().c_str())) {
            error("Failed to write built-in definitions to '%s'", builtinDefs.string().c_str());
        }
    }

    void BindingModule::bindBuiltInTypes() {
        bind::type<void>("void").getType()->getUserData<DataTypeUserData>().typescriptType = "void";
        bind::type<bool>("bool").getType()->getUserData<DataTypeUserData>().typescriptType = "boolean";

        describe(bind::type<i8>("i8").getType())
            .desc(String::Format("8-bit signed integer (min: %d, max: %d)", INT8_MIN, INT8_MAX));
        describe(bind::type<i16>("i16").getType())
            .desc(String::Format("16-bit signed integer (min: %d, max: %d)", INT16_MIN, INT16_MAX));
        describe(bind::type<i32>("i32").getType())
            .desc(String::Format("32-bit signed integer (min: %d, max: %d)", INT32_MIN, INT32_MAX));
        describe(bind::type<i64>("i64").getType())
            .desc(String::Format("64-bit signed integer (min: %lld, max: %lld)", INT64_MIN, INT64_MAX));
        describe(bind::type<u8>("u8").getType())
            .desc(String::Format("8-bit unsigned integer (min: %d, max: %d)", 0, UINT8_MAX));
        describe(bind::type<u16>("u16").getType())
            .desc(String::Format("16-bit unsigned integer (min: %d, max: %d)", 0, UINT16_MAX));
        describe(bind::type<u32>("u32").getType())
            .desc(String::Format("32-bit unsigned integer (min: %d, max: %d)", 0, UINT32_MAX));
        describe(bind::type<u64>("u64").getType())
            .desc(String::Format("64-bit unsigned integer (min: %llu, max: %llu)", 0, UINT64_MAX));
        describe(bind::type<f32>("f32").getType())
            .desc("32-bit floating point number (min: 1.175494351e-38, max: 3.402823466e+38)");
        describe(bind::type<f64>("f64").getType())
            .desc("64-bit floating point number (min: 2.2250738585072014e-308, max: 1.7976931348623158e+308)");

        bind::ObjectTypeBuilder<String> str = bind::type<String>("String");
        str.dtor();

        DataTypeUserData& userData = str.getType()->getUserData<DataTypeUserData>();
        userData.typescriptType    = "string";
        userData.marshaller        = new UtilsStringMarshaller(str.getType());
    }

    void BindingModule::processGlobalSymbol(SourceFileBuilder& dts, bind::ISymbol* symbol) {
        v8::Isolate* isolate           = m_runtime->getIsolate();
        v8::Local<v8::Context> context = m_runtime->getContext();
        v8::Local<v8::Object> global   = context->Global();

        switch (symbol->getSymbolType()) {
            case bind::SymbolType::Namespace:
                processNamespace((bind::Namespace*)symbol);
                emitNamespace(dts, (bind::Namespace*)symbol);
                break;
            case bind::SymbolType::Function: {
                emitFunction(dts, (bind::Function*)symbol);
                defineFunction(global, context, isolate, (bind::Function*)symbol);
                break;
            }
            case bind::SymbolType::DataType: {
                bind::DataType* dataType = (bind::DataType*)symbol;
                emitDataType(dts, dataType);
                defineDataType(global, context, isolate, dataType);
                break;
            }
            case bind::SymbolType::Value: {
                defineValue(global, context, isolate, nullptr, (bind::ValuePointer*)symbol);
                break;
            }
            default: break;
        }
    }

    void BindingModule::defineFunction(
        v8::Local<v8::Object>& target, v8::Local<v8::Context>& context, v8::Isolate* isolate, bind::Function* function
    ) {
        target
            ->Set(
                context,
                v8::String::NewFromUtf8(isolate, function->getName().c_str()).ToLocalChecked(),
                processFunction(function)
            )
            .Check();
    }

    void BindingModule::defineDataType(
        v8::Local<v8::Object>& target, v8::Local<v8::Context>& context, v8::Isolate* isolate, bind::DataType* dataType
    ) {
        v8::Local<v8::Value> val = processDataType(dataType);
        if (!val.IsEmpty()) {
            target->Set(context, v8::String::NewFromUtf8(isolate, dataType->getName().c_str()).ToLocalChecked(), val)
                .Check();
        }
    }

    void BindingModule::defineValue(
        v8::Local<v8::Object>& target,
        v8::Local<v8::Context>& context,
        v8::Isolate* isolate,
        bind::Namespace* ns,
        bind::ValuePointer* value
    ) {
        const bind::type_meta& meta = value->getType()->getInfo();
        DataTypeUserData& userData  = value->getType()->getUserData<DataTypeUserData>();
        if (!userData.marshaller) {
            error(
                "No marshaller found for data type '%s' of value '%s' from namespace '%s'",
                value->getType()->getName().c_str(),
                value->getName().c_str(),
                ns ? ns->getName().c_str() : "global"
            );
            return;
        }

        v8::Local<v8::Value> val;
        {
            CallContext cctx(isolate, context);
            val = userData.marshaller->toV8(cctx, value->getAddress());

            if (cctx.didAllocate()) {
                error(
                    "Marshaller for data type '%s' of value '%s' from namespace '%s' had to allocate "
                    "memory in order to convert the provided value. This is currently unsupported, "
                    "since this memory is allocated on the stack and will be freed before the lifetime "
                    "of the bound value ends. The value will not be bound as a result.",
                    value->getType()->getName().c_str(),
                    value->getName().c_str(),
                    ns ? ns->getName().c_str() : "global"
                );
            }
        }

        target->Set(context, v8::String::NewFromUtf8(isolate, value->getName().c_str()).ToLocalChecked(), val).Check();
    }

    void BindingModule::processNamespace(bind::Namespace* ns) {
        if (ns->getCorrespondingType()) {
            return;
        }

        debug("Binding global namespace '%s' as module", ns->getName().c_str());

        v8::Isolate* isolate           = m_runtime->getIsolate();
        v8::Local<v8::Context> context = m_runtime->getContext();
        v8::Local<v8::Object> exports  = v8::Object::New(isolate);

        const Array<bind::ISymbol*>& symbols = ns->getSymbols();
        for (bind::ISymbol* symbol : symbols) {
            switch (symbol->getSymbolType()) {
                case bind::SymbolType::Function:
                    defineFunction(exports, context, isolate, (bind::Function*)symbol);
                    break;
                case bind::SymbolType::DataType: {
                    defineDataType(exports, context, isolate, (bind::DataType*)symbol);
                    break;
                }
                case bind::SymbolType::Value: {
                    defineValue(exports, context, isolate, ns, (bind::ValuePointer*)symbol);
                    break;
                }
                default: break;
            }
        }

        if (ns->getName() == "process") {
            v8::Local<v8::Object> env = v8::Object::New(isolate);
            builtin::process::populateEnv(isolate, env);
            exports->Set(context, v8::String::NewFromUtf8(isolate, "env").ToLocalChecked(), env).Check();
        }

        m_runtime->registerBuiltInModule(ns->getName(), exports);
    }

    v8::Local<v8::Function> BindingModule::processFunction(bind::Function* function) {
        v8::Isolate* isolate           = m_runtime->getIsolate();
        v8::Local<v8::Context> context = m_runtime->getContext();

        FunctionUserData& userData  = function->getUserData<FunctionUserData>();
        FunctionDocumentation* docs = userData.documentation;
        bool isAsync                = docs ? docs->isAsync() : false;

        return v8::Function::New(
                   context,
                   isAsync ? AsyncFunctionCallProxy : FunctionCallProxy,
                   v8::External::New(isolate, function),
                   0,
                   v8::ConstructorBehavior::kThrow
        )
            .ToLocalChecked();
    }

    v8::Local<v8::Value> BindingModule::processDataType(bind::DataType* dataType) {
        const bind::type_meta& meta = dataType->getInfo();
        if (meta.size == 0) {
            return v8::Local<v8::Value>();
        }
        if (meta.is_pointer) {
            return v8::Local<v8::Value>();
        }
        if (meta.is_primitive && !meta.is_enum) {
            return v8::Local<v8::Value>();
        }
        if (meta.is_opaque) {
            return v8::Local<v8::Value>();
        }

        DataTypeUserData& userData = dataType->getUserData<DataTypeUserData>();
        if (userData.typescriptType) {
            return v8::Local<v8::Value>();
        }

        v8::Isolate* isolate           = m_runtime->getIsolate();
        v8::Local<v8::Context> context = m_runtime->getContext();

        if (meta.is_enum) {
            v8::Local<v8::Object> obj = v8::Object::New(isolate);

            bind::EnumType* enumType                   = (bind::EnumType*)dataType;
            const Array<bind::EnumType::Field>& fields = enumType->getFields();
            for (const bind::EnumType::Field& field : fields) {
                if (meta.is_unsigned) {
                    obj->Set(
                           context,
                           v8::String::NewFromUtf8(isolate, field.name.c_str()).ToLocalChecked(),
                           v8::Number::New(isolate, (f64)field.value.u)
                    )
                        .Check();
                } else {
                    obj->Set(
                           context,
                           v8::String::NewFromUtf8(isolate, field.name.c_str()).ToLocalChecked(),
                           v8::Number::New(isolate, (f64)field.value.i)
                    )
                        .Check();
                }
            }

            return obj;
        }

        if (meta.is_trivially_constructible == 0) {
            v8::Local<v8::FunctionTemplate> ctor = buildPrototype(isolate, dataType);
            userData.javascriptData              = new JavaScriptTypeData();
            userData.javascriptData->constructor.Reset(isolate, ctor);
            userData.hostObjectManager = new HostObjectManager(dataType);
            addNestedLogger(userData.hostObjectManager);

            return ctor->GetFunction(context).ToLocalChecked();
        }

        return v8::Local<v8::Value>();
    }

    void BindingModule::emitNamespace(SourceFileBuilder& builder, bind::Namespace* ns) {
        builder.line("declare module \"%s\" {", ns->getName().c_str());
        builder.indent();

        emitNamespaceImports(builder, ns);

        const Array<bind::ISymbol*>& symbols = ns->getSymbols();
        for (bind::ISymbol* symbol : symbols) {
            switch (symbol->getSymbolType()) {
                case bind::SymbolType::Function: emitFunction(builder, (bind::Function*)symbol, true); break;
                case bind::SymbolType::DataType: {
                    bind::DataType* dataType = (bind::DataType*)symbol;
                    emitDataType(builder, dataType, true);
                    break;
                }
                case bind::SymbolType::Value: {
                    bind::ValuePointer* value = (bind::ValuePointer*)symbol;
                    builder.line("const %s: %s;", value->getName().c_str(), getTypeName(value->getType()).c_str());
                    break;
                }
                default: break;
            }
        }

        if (ns->getName() == "process") {
            builder.line("/**");
            builder.line(" * String map of environment variables available when the process was started");
            builder.line(" */");
            builder.line("const env: { readonly [key: string]: string };");
        }

        builder.unindent();
        builder.line("}");
    }

    void BindingModule::eniUseDataType(
        utils::Array<ImportModule>& importModules, bind::DataType* dataType, bind::Namespace* ns
    ) {
        dataType = resolveType(dataType);

        DataTypeUserData& userData = dataType->getUserData<DataTypeUserData>();
        if (userData.typescriptType) {
            // The type is intrinsic
            return;
        }

        if (userData.arrayElementType) {
            eniUseDataType(importModules, userData.arrayElementType, ns);
            return;
        }

        const bind::type_meta& meta = dataType->getInfo();
        if (meta.is_function) {
            bind::FunctionType* sig = (bind::FunctionType*)dataType;
            eniUseDataType(importModules, sig->getReturnType(), ns);

            const utils::Array<bind::FunctionType::Argument>& args = sig->getArgs();
            for (const bind::FunctionType::Argument& arg : args) {
                eniUseDataType(importModules, arg.type, ns);
            }
            return;
        }

        bind::Namespace* typeNamespace = dataType->getOwnNamespace()->getParent();
        if (!typeNamespace || typeNamespace == ns || typeNamespace == bind::Registry::GlobalNamespace()) {
            return;
        }

        bool foundNamespace = false;
        for (ImportModule& mod : importModules) {
            if (mod.ns != typeNamespace) {
                continue;
            }

            foundNamespace   = true;
            bool foundImport = false;
            for (bind::DataType* import : mod.imports) {
                if (import == dataType) {
                    foundImport = true;
                    break;
                }
            }

            if (!foundImport) {
                mod.imports.push(dataType);
            }

            break;
        }

        if (!foundNamespace) {
            importModules.push({});
            ImportModule& mod = importModules.last();
            mod.ns            = typeNamespace;
            mod.imports.push(dataType);
        }
    }

    void BindingModule::emitNamespaceImports(SourceFileBuilder& builder, bind::Namespace* ns) {
        utils::Array<ImportModule> importModules;

        const Array<bind::ISymbol*>& symbols = ns->getSymbols();
        for (bind::ISymbol* symbol : symbols) {
            switch (symbol->getSymbolType()) {
                case bind::SymbolType::Function: {
                    bind::Function* func = (bind::Function*)symbol;
                    eniUseDataType(importModules, func->getSignature(), ns);
                    break;
                }
                case bind::SymbolType::DataType: {
                    bind::DataType* dataType    = (bind::DataType*)symbol;
                    const bind::type_meta& meta = dataType->getInfo();
                    DataTypeUserData& userData  = dataType->getUserData<DataTypeUserData>();

                    if (userData.typescriptType || meta.is_function || meta.is_pointer || meta.is_enum ||
                        meta.is_primitive) {
                        // these conditions mean the type won't be emitted (at least not outside of a comment)
                        continue;
                    }

                    if (userData.arrayElementType) {
                        eniUseDataType(importModules, userData.arrayElementType, ns);
                        continue;
                    }

                    const Array<bind::DataType::BaseType>& bases = dataType->getBases();
                    for (const bind::DataType::BaseType& base : bases) {
                        eniUseDataType(importModules, base.type, ns);
                    }

                    const utils::Array<bind::DataType::Property>& properties = dataType->getProps();
                    for (const bind::DataType::Property& property : properties) {
                        if (meta.is_trivially_constructible && property.offset < 0) {
                            continue;
                        }

                        eniUseDataType(importModules, property.type, ns);
                    }
                    break;
                }
                case bind::SymbolType::Value: {
                    bind::ValuePointer* value = (bind::ValuePointer*)symbol;
                    eniUseDataType(importModules, value->getType(), ns);
                    break;
                }
                default: break;
            }
        }

        for (ImportModule& mod : importModules) {
            if (mod.imports.size() == 0) {
                continue;
            }

            String importList;

            for (bind::DataType* import : mod.imports) {
                if (importList.size() > 0) {
                    importList += ", ";
                }
                importList += getTypeName(import);
            }

            builder.line("import { %s } from '%s';", importList.c_str(), mod.ns->getName().c_str());
        }
    }

    void BindingModule::emitFunction(SourceFileBuilder& builder, bind::Function* function, bool isWithinNamespace) {
        bind::FunctionType* sig    = function->getSignature();
        bind::DataType* returnType = resolveType(sig->getReturnType());

        FunctionUserData& userData  = function->getUserData<FunctionUserData>();
        FunctionDocumentation* docs = userData.documentation;
        bool isAsync                = docs ? docs->isAsync() : false;

        String parameters = getArgList(function);

        emitFunctionDocs(builder, function);

        builder.line(
            isAsync ? "%sfunction %s(%s): Promise<%s%s>;" : "%sfunction %s(%s): %s%s;",
            isWithinNamespace ? "" : "declare ",
            function->getName().c_str(),
            parameters.size() == 0 ? "" : parameters.c_str(),
            getTypeName(returnType).c_str(),
            docs && docs->returnIsNullable() ? " | null" : ""
        );
    }

    void BindingModule::emitFunctionDocs(SourceFileBuilder& builder, bind::Function* function) {
        FunctionUserData& userData = function->getUserData<FunctionUserData>();
        if (!userData.documentation) {
            return;
        }

        const FunctionDocumentation& docs = *userData.documentation;

        const String& funcDesc = docs.desc();

        builder.line("/**");
        if (funcDesc.size() > 0) {
            builder.line(" * %s", funcDesc.c_str());
        }

        const Array<FunctionDocumentation::ParameterDocs>& params = docs.params();
        for (const FunctionDocumentation::ParameterDocs& param : params) {
            builder.line(" * @param %s %s", param.name.c_str(), param.description.c_str());
        }

        const String& returnDesc = docs.returns();
        if (returnDesc.size() > 0) {
            builder.line(" * @return %s", returnDesc.c_str());
        }

        builder.line(" */");
    }

    void BindingModule::emitDataType(SourceFileBuilder& builder, bind::DataType* dataType, bool isWithinNamespace) {
        const bind::type_meta& meta = dataType->getInfo();
        DataTypeUserData& userData  = dataType->getUserData<DataTypeUserData>();

        if (userData.arrayElementType) {
            // References to this type will be emitted in place like "<element_type>[]"
            return;
        }

        if (userData.typescriptType) {
            // Already built into the language
            return;
        }

        if (meta.is_function) {
            // References to this type will be emitted in place like "(args...) => <return_type>"
            return;
        }

        if (meta.is_pointer) {
            // References to this type will be emitted in place later as the pointed to type
            return;
        }

        DataTypeDocumentation* docs = userData.documentation;

        if (docs && docs->desc().size() > 0) {
            builder.line("/**");
            builder.line(" * %s", docs->desc().c_str());
            builder.line(" */");
        }

        if (meta.is_enum) {
            bind::EnumType* enumType = (bind::EnumType*)dataType;
            builder.line("%senum %s {", isWithinNamespace ? "" : "declare ", enumType->getName().c_str());
            builder.indent();

            const Array<bind::EnumType::Field>& fields = enumType->getFields();
            for (u32 i = 0; i < fields.size(); i++) {
                const bind::EnumType::Field& field              = fields[i];
                const DataTypeDocumentation::PropertyDocs* prop = docs ? docs->property(field.name) : nullptr;
                if (prop) {
                    builder.line("/**");
                    builder.line(" * %s", prop->description.c_str());
                    builder.line(" */");
                }

                builder.line("%s = %d%s", field.name.c_str(), field.value.i, i < fields.size() - 1 ? "," : "");
            }

            builder.unindent();
            builder.line("}");
        } else if (meta.is_primitive) {
            builder.line(
                "%stype %s = number & IntrinsicNumber<%d, %s, %s>;",
                isWithinNamespace ? "" : "declare ",
                dataType->getName().c_str(),
                meta.size * 8,
                meta.is_unsigned ? "false" : "true",
                meta.is_floating_point ? "true" : "false"
            );
        } else if (meta.is_trivially_constructible) {
            builder.line("%stype %s = {", isWithinNamespace ? "" : "declare ", dataType->getName().c_str());
            builder.indent();

            const Array<bind::DataType::Property>& properties = dataType->getProps();
            for (const bind::DataType::Property& property : properties) {
                if (property.offset < 0) {
                    continue;
                }

                const DataTypeDocumentation::PropertyDocs* prop = docs ? docs->property(property.name) : nullptr;
                if (prop) {
                    builder.line("/**");
                    builder.line(" * %s", prop->description.c_str());
                    builder.line(" */");
                }

                builder.line("%s: %s;", property.name.c_str(), getTypeName(property.type).c_str());
            }

            builder.unindent();
            builder.line("}");
        } else {
            String implements;
            const Array<bind::DataType::BaseType>& bases = dataType->getBases();
            if (bases.size() > 0) {
                implements += " implements ";

                for (u32 i = 0; i < bases.size(); i++) {
                    if (i > 0) {
                        implements += ", ";
                    }

                    implements += getTypeName(bases[i].type);
                }
            }

            builder.line(
                "%sclass %s%s {",
                isWithinNamespace ? "" : "declare ",
                dataType->getName().c_str(),
                implements.size() > 0 ? implements.c_str() : ""
            );
            builder.indent();

            const Array<bind::DataType::Property>& properties = dataType->getProps();
            // first static properties
            for (const bind::DataType::Property& property : properties) {
                if (property.flags.is_static == 0 || !property.address.get()) {
                    continue;
                }

                if (property.flags.is_method == 1 || property.flags.is_pseudo_method == 1) {
                    continue;
                }

                const DataTypeDocumentation::PropertyDocs* prop = docs ? docs->property(property.name) : nullptr;
                if (prop) {
                    builder.line("/**");
                    builder.line(" * %s", prop->description.c_str());
                    builder.line(" */");
                }

                builder.line("static %s: %s;", property.name.c_str(), getTypeName(property.type).c_str());
            }

            // then regular properties
            for (const bind::DataType::Property& property : properties) {
                if (property.offset < 0) {
                    continue;
                }

                const DataTypeDocumentation::PropertyDocs* prop = docs ? docs->property(property.name) : nullptr;
                if (prop) {
                    builder.line("/**");
                    builder.line(" * %s", prop->description.c_str());
                    builder.line(" */");
                }

                builder.line(
                    "%s%s: %s;",
                    property.flags.can_write == 0 ? "readonly " : "",
                    property.name.c_str(),
                    getTypeName(property.type).c_str()
                );
            }

            // then constructors
            for (const bind::DataType::Property& property : properties) {
                if (property.flags.is_ctor == 0) {
                    continue;
                }

                bind::Function* func = (bind::Function*)property.address.get();
                emitFunctionDocs(builder, func);

                bind::FunctionType* sig    = func->getSignature();
                bind::DataType* returnType = resolveType(sig->getReturnType());

                String parameters = getArgList(func);
                builder.line("constructor(%s);", parameters.size() == 0 ? "" : parameters.c_str());
            }

            // then non-static methods
            for (const bind::DataType::Property& property : properties) {
                if (property.flags.is_static == 1) {
                    continue;
                }

                if (property.flags.is_method == 0 && property.flags.is_pseudo_method == 0) {
                    continue;
                }

                bind::Function* func = (bind::Function*)property.address.get();
                emitFunctionDocs(builder, func);

                FunctionUserData& userData   = func->getUserData<FunctionUserData>();
                FunctionDocumentation* fdocs = userData.documentation;
                bool isAsync                 = fdocs ? fdocs->isAsync() : false;

                bind::FunctionType* sig    = func->getSignature();
                bind::DataType* returnType = resolveType(sig->getReturnType());

                String parameters = getArgList(func);

                builder.line(
                    isAsync ? "%s(%s): Promise<%s%s>;" : "%s(%s): %s%s;",
                    property.name.c_str(),
                    parameters.size() == 0 ? "" : parameters.c_str(),
                    getTypeName(returnType).c_str(),
                    fdocs && fdocs->returnIsNullable() ? " | null" : ""
                );
            }

            // then static methods
            for (const bind::DataType::Property& property : properties) {
                if (property.flags.is_static == 0) {
                    continue;
                }

                if (property.flags.is_method == 0 && property.flags.is_pseudo_method == 0) {
                    continue;
                }

                bind::Function* func = (bind::Function*)property.address.get();
                emitFunctionDocs(builder, func);

                bind::FunctionType* sig    = func->getSignature();
                bind::DataType* returnType = resolveType(sig->getReturnType());

                String parameters = getArgList(func);

                FunctionUserData& userData   = func->getUserData<FunctionUserData>();
                FunctionDocumentation* fdocs = userData.documentation;
                bool isAsync                 = fdocs ? fdocs->isAsync() : false;

                builder.line(
                    isAsync ? "static %s(%s): Promise<%s%s>;" : "static %s(%s): %s%s;",
                    property.name.c_str(),
                    parameters.size() == 0 ? "" : parameters.c_str(),
                    getTypeName(returnType).c_str(),
                    fdocs && fdocs->returnIsNullable() ? " | null" : ""
                );
            }

            // then the magic built-in stuff
            builder.line("/**");
            builder.line(" * This should be used to destroy host objects which were allocated by a script");
            builder.line(" */");
            builder.line("destroy(): void;");

            builder.unindent();
            builder.line("}");
        }
    }

    bind::DataType* BindingModule::resolveType(bind::DataType* dataType) {
        const bind::type_meta& meta = dataType->getInfo();

        if (meta.is_pointer && !meta.is_function) {
            return resolveType(((bind::PointerType*)dataType)->getDestinationType());
        }

        return dataType;
    }

    String BindingModule::getTypeName(bind::DataType* dataType) {
        dataType = resolveType(dataType);

        const DataTypeUserData& userData = dataType->getUserData<DataTypeUserData>();
        if (userData.typescriptType) {
            return userData.typescriptType;
        }

        if (userData.arrayElementType) {
            return String::Format("%s[]", getTypeName(userData.arrayElementType).c_str());
        }

        const bind::type_meta& meta = dataType->getInfo();
        if (meta.size == 0) {
            return "void";
        }

        if (meta.is_function) {
            bind::FunctionType* sig    = ((bind::FunctionType*)dataType);
            bind::DataType* returnType = resolveType(sig->getReturnType());

            String parameters;
            const Array<bind::FunctionType::Argument>& args = sig->getArgs();
            for (u32 i = 0; i < args.size(); i++) {
                if (i > 0) {
                    parameters += ", ";
                }
                parameters += String::Format("param_%d: %s", i + 1, getTypeName(args[i].type).c_str());
            }

            return String::Format(
                "((%s) => %s)", parameters.size() == 0 ? "" : parameters.c_str(), getTypeName(returnType).c_str()
            );
        }

        return dataType->getName();
    }

    String BindingModule::getArgList(bind::Function* func) {
        FunctionUserData& userData  = func->getUserData<FunctionUserData>();
        FunctionDocumentation* docs = userData.documentation;

        String parameters;
        ConstArrayView<bind::FunctionType::Argument> args = func->getExplicitArgs();
        for (u32 i = 0; i < args.size(); i++) {
            if (i > 0) {
                parameters += ", ";
            }
            const FunctionDocumentation::ParameterDocs* param = docs ? docs->param(i) : nullptr;
            if (param) {
                parameters += String::Format(
                    "%s: %s%s",
                    param->name.c_str(),
                    getTypeName(args[i].type).c_str(),
                    param->isNullable ? " | null" : ""
                );
            } else {
                parameters += String::Format("param_%d: %s", i + 1, getTypeName(args[i].type).c_str());
            }
        }

        return parameters;
    }
}
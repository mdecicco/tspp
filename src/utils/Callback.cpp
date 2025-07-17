#include <tspp/utils/Callback.h>
#include <tspp/utils/CallContext.h>
#include <tspp/interfaces/IDataMarshaller.h>
#include <bind/FunctionType.h>
#include <utils/Exception.h>

namespace tspp {
    std::unordered_map<void*, Callback*> Callback::s_map = {};
    MemoryPool Callback::m_pool = MemoryPool(sizeof(Callback), 1024, false);

    void invokeCallback(ffi_cif* cif, void* ret, void** args, void* user_data);

    Callback::Callback(
        v8::Isolate* isolate,
        void* closure,
        bind::FunctionType* sig,
        const v8::Local<v8::Function>& target
    ) {
        m_isolate = isolate;
        m_closure = closure;
        m_sig = sig;
        m_target.Reset(isolate, target);
        m_refCount = 1;
    }

    Callback::~Callback() {
        if (m_closure) {
            ffi_closure_free(m_closure);
            m_closure = nullptr;
            m_target.Reset();
        }
    }

    v8::Isolate* Callback::getIsolate() const {
        return m_isolate;
    }

    v8::Local<v8::Function> Callback::getTarget() const {
        return m_target.Get(m_isolate);
    }

    bind::FunctionType* Callback::getSig() const {
        return m_sig;
    }

    void Callback::AddRef(void* callback) {
        auto it = s_map.find(callback);
        if (it == s_map.end()) {
            throw Exception("Attempted to add reference to unbound callback");
        }

        it->second->m_refCount++;
    }

    void Callback::Release(void* callback) {
        auto it = s_map.find(callback);
        if (it == s_map.end()) {
            throw Exception("Attempted to release unbound callback");
        }

        Callback* cb = it->second;
        cb->m_refCount--;

        if (cb->m_refCount == 0) {
            cb->~Callback();
            m_pool.free(cb);
            s_map.erase(it);
        }
    }

    void* Callback::Create(v8::Isolate* isolate, bind::FunctionType* sig, const v8::Local<v8::Function>& target) {
        void* fptr = nullptr;
        void* closure = ffi_closure_alloc(sizeof(ffi_closure), &fptr);
        if (!closure) return nullptr;

        Callback* cb = (Callback*)m_pool.alloc();
        ffi_cif* cif = sig->getCif();
        if (ffi_prep_closure_loc(
            (ffi_closure*)closure,
            cif,
            invokeCallback,
            cb,
            fptr
        ) != FFI_OK) {
            m_pool.free(cb);
            ffi_closure_free(closure);
            return nullptr;
        }

        new (cb) Callback(isolate, closure, sig, target);
        s_map.insert({ fptr, cb });
        
        return fptr;
    }

    void Callback::DestroyAll() {
        for (auto it = s_map.begin(); it != s_map.end(); ++it) {
            it->second->~Callback();
        }

        m_pool.reset();
        s_map.clear();
    }

    void invokeCallback(ffi_cif* cif, void* ret, void** args, void* user_data) {
        Callback* cb = (Callback*)user_data;
        bind::FunctionType* sig = cb->getSig();

        v8::Isolate* isolate = cb->getIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        v8::Local<v8::Function> target = cb->getTarget();

        v8::TryCatch tryCatch(isolate);
        v8::Local<v8::Value> destArgs[16];

        const Array<bind::FunctionType::Argument>& sigArgs = sig->getArgs();
        CallContext callCtx(isolate, context);
        for (u32 i = 0; i < sigArgs.size(); i++) {
            bind::DataType* type = sigArgs[i].type;
            DataTypeUserData& data = type->getUserData<DataTypeUserData>();
            destArgs[i] = data.marshaller->toV8(callCtx, args[i]);

            if (tryCatch.HasCaught()) {
                v8::String::Utf8Value error(isolate, tryCatch.Exception());
                throw Exception(*error);
            }
        }

        v8::MaybeLocal<v8::Value> result = target->Call(context, context->Global(), sigArgs.size(), destArgs);
        if (tryCatch.HasCaught()) {
            v8::String::Utf8Value error(isolate, tryCatch.Exception());
            throw Exception(*error);
        }

        bind::DataType* retType = sig->getReturnType();
        DataTypeUserData& retData = retType->getUserData<DataTypeUserData>();

        if (retType->getInfo().size > 0) {
            if (result.IsEmpty()) {
                throw Exception("Callback did not return a value when one was expected");
            }

            v8::Local<v8::Value> retVal;
            if (!result.ToLocal(&retVal)) {
                throw Exception("Failed to get return value from callback");
            }

            callCtx.setNextAllocation((u8*)ret);
            retData.marshaller->fromV8(callCtx, retVal);
            
            if (tryCatch.HasCaught()) {
                v8::String::Utf8Value error(isolate, tryCatch.Exception());
                throw Exception(*error);
            }
        }
    }
}
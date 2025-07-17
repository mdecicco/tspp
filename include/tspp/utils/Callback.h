#pragma once
#include <tspp/types.h>
#include <utils/MemoryPool.h>

#include <unordered_map>
#include <v8.h>

namespace bind {
    class FunctionType;
}

namespace tspp {
    class Callback {
        public:
            v8::Isolate* getIsolate() const;
            v8::Local<v8::Function> getTarget() const;
            bind::FunctionType* getSig() const;

            static void AddRef(void* callback);
            static void Release(void* callback);
            static void* Create(
                v8::Isolate* isolate,
                bind::FunctionType* sig,
                const v8::Local<v8::Function>& target
            );

            static void DestroyAll();
        
        private:
            Callback(
                v8::Isolate* isolate,
                void* closure,
                bind::FunctionType* sig,
                const v8::Local<v8::Function>& target
            );
            ~Callback();

            v8::Isolate* m_isolate;
            void* m_closure;
            bind::FunctionType* m_sig;
            v8::Global<v8::Function> m_target;
            u32 m_refCount;

            static MemoryPool m_pool;
            static std::unordered_map<void*, Callback*> s_map;
    };
}

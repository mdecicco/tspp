#pragma once
#include <tspp/types.h>
#include <tspp/interfaces/IWithLogging.h>
#include <utils/MemoryPool.h>
#include <utils/Array.h>

#include <unordered_map>
#include <v8.h>

namespace bind {
    class DataType;
    class Function;
}

namespace tspp {
    class HostObjectManager : public IWithLogging {
        public:
            HostObjectManager(bind::DataType* dataType, u32 elementsPerPool = 256);
            ~HostObjectManager();

            void* alloc(const v8::Local<v8::Object>& target);
            void* preemptiveAlloc();
            void assignTarget(void* ptr, const v8::Local<v8::Object>& target);
            void free(void* mem);
            u32 getLiveCount();
            u32 getLiveMemSize();
            v8::Local<v8::Object> getTargetIfMapped(v8::Isolate* isolate, void* mem);
        
        private:
            using ObjRef = std::unique_ptr<v8::Global<v8::Object>>;
            void bindGCListener(ObjRef& ref, void* mem);

            MemoryPool m_pool;
            std::unordered_map<void*, ObjRef> m_liveObjects;
            bind::Function* m_destructor;
            bind::DataType* m_dataType;
    };
}
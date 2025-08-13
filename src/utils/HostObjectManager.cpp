#include <bind/DataType.h>
#include <bind/Function.h>
#include <tspp/utils/HostObjectManager.h>
#include <utils/Exception.h>

namespace tspp {
    struct ObjectData_TempWorkaround {
            void* mem;
            HostObjectManager* manager;
    };

    void WeakCallback(const v8::WeakCallbackInfo<ObjectData_TempWorkaround>& info) {
        ObjectData_TempWorkaround* data = static_cast<ObjectData_TempWorkaround*>(info.GetParameter());
        data->manager->free(data->mem);
    }

    HostObjectManager::HostObjectManager(bind::DataType* dataType, u32 elementsPerPool)
        : IWithLogging(String::Format("HostObjectManager[%s]", dataType->getName().c_str())),
          m_pool(dataType->getInfo().size, elementsPerPool, false) {
        m_dataType   = dataType;
        m_destructor = dataType->getDestructor();
    }

    HostObjectManager::~HostObjectManager() {
        // TODO:
        // It may be necessary, across all memory managed marshallers, to destroy objects
        // in a specific order based on interdependencies between them...

        if (m_destructor && m_liveObjects.size() > 0) {
            void* args[] = {nullptr};
            for (auto& pair : m_liveObjects) {
                args[0] = pair.first;
                m_destructor->call(nullptr, args);

                ObjRef& ref = pair.second;

                if (ref->IsEmpty()) {
                    warn(
                        "Found allocated object with no JS object reference. "
                        "This is likely due to a call to preemptiveAlloc() without a corresponding call to "
                        "assignTarget()."
                    );
                    continue;
                }

                ref->Reset();
            }
        }
    }

    void* HostObjectManager::alloc(const v8::Local<v8::Object>& target) {
        void* mem  = m_pool.alloc();
        ObjRef ref = std::make_unique<v8::Global<v8::Object>>(target->GetIsolate(), target);

        bindGCListener(ref, mem);

        m_liveObjects.insert(std::make_pair(mem, std::move(ref)));
        return mem;
    }

    void* HostObjectManager::preemptiveAlloc() {
        void* mem  = m_pool.alloc();
        ObjRef ref = std::make_unique<v8::Global<v8::Object>>();
        m_liveObjects.insert(std::make_pair(mem, std::move(ref)));
        return mem;
    }

    void HostObjectManager::assignTarget(void* mem, const v8::Local<v8::Object>& target) {
        auto it = m_liveObjects.find(mem);
        if (it == m_liveObjects.end()) {
            error("Attempted to assign target to a memory block that was not allocated by this manager.");
            return;
        }

        ObjRef& ref = it->second;
        if (!ref->IsEmpty()) {
            error("Attempted to assign target to a memory block that already has a JS object reference.");
            return;
        }

        ref->Reset(target->GetIsolate(), target);

        bindGCListener(ref, mem);
    }

    void HostObjectManager::free(void* mem) {
        auto it = m_liveObjects.find(mem);
        if (it == m_liveObjects.end()) {
            error("Attempted to free a memory block that was not allocated by this manager.");
            return;
        }

        if (m_destructor) {
            void* args[] = {&mem};
            m_destructor->call(nullptr, args);
        } else if (m_dataType->getInfo().is_trivially_destructible == 0) {
            error("Non-trivially destructible type '%s' has no destructor.", m_dataType->getName().c_str());
        }

        m_pool.free(mem);
        m_liveObjects.erase(mem);
    }

    u32 HostObjectManager::getLiveCount() {
        return m_liveObjects.size();
    }

    u32 HostObjectManager::getLiveMemSize() {
        return m_liveObjects.size() * m_dataType->getInfo().size;
    }

    v8::Local<v8::Object> HostObjectManager::getTargetIfMapped(v8::Isolate* isolate, void* mem) {
        auto it = m_liveObjects.find(mem);
        if (it == m_liveObjects.end()) {
            return v8::Local<v8::Object>();
        }

        ObjRef& ref = it->second;
        if (ref->IsEmpty()) {
            warn(
                "Found allocated object with no JS object reference. "
                "This is likely due to a call to preemptiveAlloc() without a corresponding call to assignTarget()."
            );
            return v8::Local<v8::Object>();
        }

        return ref->Get(isolate);
    }

    void HostObjectManager::bindGCListener(ObjRef& ref, void* mem) {
        ObjectData_TempWorkaround* data = new ObjectData_TempWorkaround({mem, this});
        ref->SetWeak(data, WeakCallback, v8::WeakCallbackType::kParameter);
    }
}
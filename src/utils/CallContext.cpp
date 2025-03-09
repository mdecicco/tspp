#include <tspp/utils/CallContext.h>
#include <bind/DataType.h>
#include <bind/Function.h>
#include <utils/Array.hpp>

namespace tspp {
    CallContext::CallContext(v8::Isolate* isolate, const v8::Local<v8::Context>& context) {
        m_isolate = isolate;
        m_context = context;
        m_nextAllocationOverride = nullptr;
    }

    CallContext::~CallContext() {
        for (Allocation& allocation : m_allocations) {
            bind::Function* dtor = allocation.type->getDestructor();
            if (dtor) {
                void* args[] = {
                    &allocation.data
                };
                dtor->call(nullptr, args);
            }
            
            delete [] allocation.data;
        }

        m_allocations.clear();
    }

    v8::Isolate* CallContext::getIsolate() const {
        return m_isolate;
    }

    v8::Local<v8::Context> CallContext::getContext() const {
        return m_context;
    }

    void CallContext::setNextAllocation(u8* data) {
        m_nextAllocationOverride = data;
    }

    bool CallContext::hasAllocationTarget() const {
        return m_nextAllocationOverride != nullptr;
    }

    u8* CallContext::alloc(bind::DataType* dataType) {
        if (m_nextAllocationOverride) {
            u8* old = m_nextAllocationOverride;
            m_nextAllocationOverride = nullptr;
            return old;
        }

        Allocation allocation;
        allocation.type = dataType;
        allocation.data = new u8[dataType->getInfo().size];
        m_allocations.push(allocation);
        return allocation.data;
    }
}
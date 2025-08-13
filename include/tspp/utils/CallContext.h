#pragma once
#include <tspp/types.h>
#include <utils/Array.h>

#include <v8.h>

namespace bind {
    class DataType;
}

namespace tspp {
    class CallContext {
        public:
            CallContext(v8::Isolate* isolate, const v8::Local<v8::Context>& context);
            ~CallContext();

            /**
             * @brief Gets the associated isolate
             * @return The isolate
             */
            v8::Isolate* getIsolate() const;

            /**
             * @brief Gets the associated context
             * @return The context
             */
            v8::Local<v8::Context> getContext() const;

            /**
             * @brief Sets the next allocation pointer
             * @param data Pointer that will be used for the next allocation
             */
            void setNextAllocation(u8* data);

            /**
             * @brief Checks if there is a specific allocation target set. That is,
             * if the next allocation will be written to a specific location rather
             * than being allocated automatically if necessary.
             *
             * @return True if there is an allocation target, false otherwise
             */
            bool hasAllocationTarget() const;

            /**
             * @brief Allocates memory for a data type
             * @param dataType The data type to allocate memory for
             * @return Pointer to the allocated memory
             */
            u8* alloc(bind::DataType* dataType);

            /**
             * @brief Adds a callback to the context
             * @param callback The callback to add
             */
            void addCallback(void* callback);

            /**
             * @brief Checks if the context has allocated any memory which will be freed with the call context.
             * @return True if the context has allocated any memory, false otherwise
             */
            bool didAllocate() const;

        private:
            struct Allocation {
                    bind::DataType* type;
                    u8* data;
            };

            Array<Allocation> m_allocations;
            Array<void*> m_callbacks;
            u8* m_nextAllocationOverride;
            v8::Isolate* m_isolate;
            v8::Local<v8::Context> m_context;
    };
}
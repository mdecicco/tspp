#pragma once
#include <tspp/types.h>

namespace tspp::builtin::databuffer {
    class DataBuffer {
        public:
            DataBuffer(u64 size);
            DataBuffer(DataBuffer& other);
            ~DataBuffer();

            u8* data() const;
            u64 size() const;

        protected:
            u8* m_data;
            u64 m_size;
    };

    void init();
}
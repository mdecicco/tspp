#include <tspp/builtin/databuffer.h>
#include <tspp/marshalling/DataBufferMarshaller.h>
#include <tspp/utils/Docs.h>
#include <tspp/bind.h>
using namespace bind;

namespace tspp::builtin::databuffer {
    DataBuffer::DataBuffer(u64 size) {
        m_size = size;
        m_data = nullptr;

        if (m_size == 0) return;
        m_data = new u8[size];
    }

    DataBuffer::DataBuffer(DataBuffer& other) {
        m_size = other.m_size;
        m_data = other.m_data;

        other.m_size = 0;
        other.m_data = nullptr;
    }

    DataBuffer::~DataBuffer() {
        if (m_data) delete[] m_data;
        m_data = nullptr;
        m_size = 0;
    }

    u8* DataBuffer::data() const {
        return m_data;
    }

    u64 DataBuffer::size() const {
        return m_size;
    }

    String decodeUTF8(const DataBuffer& buffer) {
        u64 sz = buffer.size();
        if (sz == 0) return "";

        u64 length = sz;
        char* data = (char*)buffer.data();
        if (data[length - 1] == EOF) length--;
        
        return String::View(data, length);
    }

    void init() {
        Namespace* ns = new Namespace("__internal:buffer");
        Registry::Add(ns);

        ObjectTypeBuilder<DataBuffer> builder = ns->type<DataBuffer>("DataBuffer");
        describe(builder.ctor<u64>())
            .desc("Creates a new DataBuffer with the specified size")
            .param(0, "size", "The size of the DataBuffer");
        describe(builder.ctor<DataBuffer&>())
            .desc("Creates a new DataBuffer by copying another DataBuffer")
            .param(0, "buffer", "The DataBuffer to copy");
        
        builder.dtor();

        DataType* type = builder.getType();
        DataTypeUserData& userData = type->getUserData<DataTypeUserData>();
        userData.typescriptType = "ArrayBuffer";
        userData.marshaller = new DataBufferMarshaller(type);

        describe(ns->function("decodeUTF8", decodeUTF8))
            .desc("Decodes an ArrayBuffer as a UTF-8 string")
            .param(0, "buffer", "The ArrayBuffer to decode")
            .returns("The decoded UTF-8 string");
    }
}
#pragma once
#include <tspp/types.h>
#include <utils/String.h>
#include <utils/Array.h>
#include <tspp/bind.h>

namespace tspp {
    class FunctionDocumentation {
        public:
            struct ParameterDocs {
                u32 parameterIndex;
                String name;
                String description;
            };

            FunctionDocumentation(bind::Function* function);
            ~FunctionDocumentation();

            FunctionDocumentation& desc(const String& description);
            FunctionDocumentation& param(u32 index, const String& name, const String& description);
            FunctionDocumentation& returns(const String& description);
            FunctionDocumentation& async();

            const String& desc() const;
            const String& returns() const;
            const Array<ParameterDocs>& params() const;
            const ParameterDocs* param(u32 index) const;
            bool isAsync() const;

        private:
            u32 m_paramCount;
            String m_description;
            String m_returnDesc;
            Array<ParameterDocs> m_parameters;
            bool m_isAsync;
    };

    class DataTypeDocumentation {
        public:
            struct PropertyDocs {
                String name;
                String description;
            };

            DataTypeDocumentation();
            ~DataTypeDocumentation();

            DataTypeDocumentation& desc(const String& description);
            DataTypeDocumentation& property(const String& name, const String& description);

            const String& desc() const;
            const Array<PropertyDocs>& properties() const;
            const PropertyDocs* property(const String& name) const;

        private:
            String m_description;
            Array<PropertyDocs> m_properties;
    };

    FunctionDocumentation& describe(bind::Function* func);
    FunctionDocumentation& describe(const bind::DataType::Property& method);
    DataTypeDocumentation& describe(bind::DataType* dataType);
}


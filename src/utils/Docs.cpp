#include <tspp/utils/Docs.h>
#include <bind/Function.h>
#include <utils/Array.hpp>
#include <utils/Exception.h>

namespace tspp {
    FunctionDocumentation::FunctionDocumentation(bind::Function* function) {
        m_paramCount = function->getExplicitArgs().size();
        m_isAsync = false;
    }

    FunctionDocumentation::~FunctionDocumentation() {
    }

    FunctionDocumentation& FunctionDocumentation::desc(const String& description) {
        m_description = description;
        return *this;
    }

    FunctionDocumentation& FunctionDocumentation::param(u32 index, const String& name, const String& description) {
        if (index >= m_paramCount) {
            throw Exception("Parameter index out of bounds");
        }

        for (u32 i = 0;i < m_parameters.size();i++) {
            if (m_parameters[i].parameterIndex == index) {
                m_parameters[i].name = name;
                m_parameters[i].description = description;
                return *this;
            }
        }

        m_parameters.push({ index, name, description });
        return *this;
    }

    FunctionDocumentation& FunctionDocumentation::returns(const String& description) {
        m_returnDesc = description;
        return *this;
    }

    FunctionDocumentation& FunctionDocumentation::async() {
        m_isAsync = true;
        return *this;
    }

    const String& FunctionDocumentation::desc() const {
        return m_description;
    }

    const String& FunctionDocumentation::returns() const {
        return m_returnDesc;
    }

    const Array<FunctionDocumentation::ParameterDocs>& FunctionDocumentation::params() const {
        return m_parameters;
    }

    const FunctionDocumentation::ParameterDocs* FunctionDocumentation::param(u32 index) const {
        for (u32 i = 0;i < m_paramCount;i++) {
            if (m_parameters[i].parameterIndex == index) {
                return &m_parameters[i];
            }
        }

        return nullptr;
    }

    bool FunctionDocumentation::isAsync() const {
        return m_isAsync;
    }





    
    DataTypeDocumentation::DataTypeDocumentation() {
    }

    DataTypeDocumentation::~DataTypeDocumentation() {
    }

    DataTypeDocumentation& DataTypeDocumentation::desc(const String& description) {
        m_description = description;
        return *this;
    }

    DataTypeDocumentation& DataTypeDocumentation::property(const String& name, const String& description) {
        for (u32 i = 0;i < m_properties.size();i++) {
            if (m_properties[i].name == name) {
                m_properties[i].description = description;
                return *this;
            }
        }

        m_properties.push({ name, description });
        return *this;
    }

    const String& DataTypeDocumentation::desc() const {
        return m_description;
    }

    const Array<DataTypeDocumentation::PropertyDocs>& DataTypeDocumentation::properties() const {
        return m_properties;
    }

    const DataTypeDocumentation::PropertyDocs* DataTypeDocumentation::property(const String& name) const {
        for (u32 i = 0;i < m_properties.size();i++) {
            if (m_properties[i].name == name) {
                return &m_properties[i];
            }
        }

        return nullptr;
    }

    FunctionDocumentation& describe(bind::Function* function) {
        FunctionUserData& userData = function->getUserData<FunctionUserData>();
        if (!userData.documentation) {
            userData.documentation = new FunctionDocumentation(function);
        }

        return *userData.documentation;
    }

    FunctionDocumentation& describe(const bind::DataType::Property& method) {
        if (method.offset >= 0) {
            throw Exception("Specified property does not refer to a function");
        }

        if (
            method.flags.is_ctor == 0 &&
            method.flags.is_dtor == 0 &&
            method.flags.is_method == 0 &&
            method.flags.is_pseudo_method == 0
        ) {
            throw Exception("Specified property does not refer to a function");
        }

        bind::Function* func = (bind::Function*)method.address.get();
        return describe(func);
    }

    DataTypeDocumentation& describe(bind::DataType* dataType) {
        DataTypeUserData& userData = dataType->getUserData<DataTypeUserData>();
        if (!userData.documentation) {
            userData.documentation = new DataTypeDocumentation();
        }

        return *userData.documentation;
    }
}
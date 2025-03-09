#include <catch2/catch_test_macros.hpp>
#include <utils/String.h>

using namespace utils;

namespace Catch {
    template<>
    struct StringMaker<String> {
        static std::string convert(String const& value) {
            return String::Format("\"%s\"", value.c_str());
        }
    };
};
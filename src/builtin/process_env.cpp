#include <utils/Exception.h>
#include <v8.h>
#include <windows.h>

namespace tspp::builtin::process {
    void populateEnv(v8::Isolate* isolate, const v8::Local<v8::Object>& env) {
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::HandleScope scope(isolate);

        LPWCH envStr = GetEnvironmentStringsW();
        if (!envStr) {
            throw utils::GenericException("Failed to get environment strings");
        }

        LPWCH cur = envStr;
        while (*cur) {
            LPWCH eq = wcschr(cur, L'=');
            if (!eq) {
                throw utils::GenericException("Invalid environment string");
            }

            std::wstring key   = std::wstring(cur, eq);
            std::wstring value = std::wstring(eq + 1);

            env->Set(
                context,
                v8::String::NewFromUtf8(isolate, std::string(key.begin(), key.end()).c_str()).ToLocalChecked(),
                v8::String::NewFromUtf8(isolate, std::string(value.begin(), value.end()).c_str()).ToLocalChecked()
            );

            cur += wcslen(cur) + 1;
        }

        FreeEnvironmentStringsW(envStr);
    }
}

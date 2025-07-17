#include <tspp/tspp.h>
#include <tspp/utils/Thread.h>

using namespace utils;
using namespace tspp;

class TestLogger : public IWithLogging {
    public:
        TestLogger() : IWithLogging("") {}

    private:
        char m_msgBuf[4096];

        bool parseTagAndMsg(char* tag, char* msg, const char* src) {
            if (src[0] != '[') return false;
            if (!isalpha(src[1])) return false;

            u32 off = 0;
            while (off < 62) {
                tag[off] = src[off];

                if (src[off] == ']' || !src[off]) break;

                off++;
            }

            off++;
            tag[off] = '\0';

            if (src[off] != ' ') return false;
            off++;

            u32 msgLen = 0;
            while (msgLen < 1023) {
                msg[msgLen] = src[off];

                if (!src[off]) break;

                msgLen++;
                off++;
            }

            msg[msgLen] = '\0';

            return true;
        }

        void onDebug(const char* msg) override {
            if (!msg) return;

            char tag[64] = { 0 };
            m_msgBuf[0] = 0;
            if (parseTagAndMsg(tag, m_msgBuf, msg)) {
                printf("\x1b[38;5;240m");
                printf("%s ", tag);
                printf("\033[32m");
                puts(m_msgBuf);
                printf("\x1b[0m");
                return;
            }
            
            printf("\033[32m%s\033[0m\n", msg);
        }

        void onInfo(const char* msg) override {
            if (!msg) return;

            char tag[64] = { 0 };
            m_msgBuf[0] = 0;
            if (parseTagAndMsg(tag, m_msgBuf, msg)) {
                printf("\x1b[38;5;240m");
                printf("%s ", tag);
                printf("\033[37m");
                puts(m_msgBuf);
                printf("\x1b[0m");
                return;
            }
            
            printf("\033[37m%s\033[0m\n", msg);
        }

        void onWarn(const char* msg) override {
            if (!msg) return;

            char tag[64] = { 0 };
            m_msgBuf[0] = 0;
            if (parseTagAndMsg(tag, m_msgBuf, msg)) {
                printf("\x1b[38;5;240m");
                printf("%s ", tag);
                printf("\033[33m");
                puts(m_msgBuf);
                printf("\x1b[0m");
                return;
            }

            printf("\033[33m%s\033[0m\n", msg);
        }

        void onError(const char* msg) override {
            if (!msg) return;

            char tag[64] = { 0 };
            m_msgBuf[0] = 0;
            if (parseTagAndMsg(tag, m_msgBuf, msg)) {
                printf("\x1b[38;5;240m");
                printf("%s ", tag);
                printf("\033[31m");
                puts(m_msgBuf);
                printf("\x1b[0m");
                return;
            }

            printf("\033[31m%s\033[0m\n", msg);
        }
};

int main(i32 argc, const char* argv[]) {
    bind::Registry::Create();
    {
        TestLogger logger;

        Runtime runtime;
        runtime.addLogHandler(&logger);

        if (runtime.initialize()) {
            runtime.commitBindings();

            if (runtime.buildProject()) {
                v8::Isolate::Scope isolateScope(runtime.getIsolate());
                v8::HandleScope scope(runtime.getIsolate());
                runtime.requireModule("src/test");
            }

            while (runtime.service()) {
            }

            runtime.shutdown();
        }
    }
    // bind::Registry::Destroy();

    return 0;
}
#include "Common.h"
#include <tspp/tspp.h>

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

struct test0 {
    i32 a;
};

struct test1 {
    i32 aa;
    test0* a;
};

struct test2 {
    test2() : a(5) {}
    ~test2() {
        printf("Destroyed test2\n");
    }

    i32 a;
};

TEST_CASE("Playground", "[tspp]") {
    bind::Registry::Create();
    {

        TestLogger logger;
        logger.logInfo("Info");
        logger.logDebug("Debug");
        logger.logWarn("Warning");
        logger.logError("Error");
        logger.logInfo("[Test] Info");
        logger.logDebug("[Test] Debug");
        logger.logWarn("[Test] Warning");
        logger.logError("[Test] Error");

        Runtime runtime;
        runtime.addLogHandler(&logger);

        if (runtime.initialize()) {
            bind::DataType* testArr = bind::Registry::GetType<Array<u32>>();

            bind::Namespace* testNS = new bind::Namespace("test");
            bind::Registry::Add(testNS);

            testNS->type<test0>("test0").prop("a", &test0::a);

            auto t1 = testNS->type<test1>("test1");
            t1.prop("aa", &test1::aa);
            t1.prop("a", &test1::a);

            auto t2 = testNS->type<test2>("test2");
            t2.ctor();
            t2.dtor();
            t2.prop("a", &test2::a);

            testNS->function("printNum", +[](i32 number) {
                return printf("It worked! %d\n", number);
            });

            testNS->function("printStr", +[](const utils::String& str) {
                return printf("It worked! %s\n", str.c_str());
            });

            testNS->function("printTest0", +[](const test0& t) {
                return printf("{ a: %d }\n", t.a);
            });

            testNS->function("printTest1", +[](const test1& t) {
                if (t.a) {
                    return printf("{ aa: %d, a: { a: %d } }\n", t.aa, t.a->a);
                }

                return printf("{ aa: %d, a: null }\n", t.aa);
            });

            testNS->function("printArr", +[](const Array<test0>& arr) {
                printf("[");
                for (u32 i = 0;i < arr.size();i++) {
                    if (i > 0) printf(", ");
                    printf("{ a: %d }", arr[i].a);
                }
                printf("]\n");
            });

            runtime.commitBindings();
            
            {
                v8::HandleScope hs(runtime.getIsolate());
                runtime.executeString(
                    "const m = require('test');\n"
                    "console.log(JSON.stringify(m));\n"
                    "console.log(m.printNum(5));\n"
                    "m.printStr('test');\n"
                    "m.printTest0({ a: 55 });\n"
                    "m.printTest1({ aa: 32, a: { a: 3 } });\n"
                    "m.printTest1({ aa: 32, a: null });\n"
                    "m.printTest1({ });\n"
                    "m.printTest1({ aa: 32 });\n"
                    "m.printArr([{ a: 1 }, { a: 2 }, { a: 3 }, { a: 4 }, { a: 5 }]);\n"
                    "const val = new m.test2()\n"
                    "m.printTest0(val);\n"
                    "m.printNum(val.a);\n"
                    "val.destroy();\n"
                    "const fs = require('__internal:fs');\n"
                    "console.log(JSON.stringify(fs.readDir('./')));\n"
                );
            }

            fflush(stdout);
            fflush(stdout);
            fflush(stdout);
            fflush(stdout);

            runtime.shutdown();
        }
    }
    // bind::Registry::Destroy();
}
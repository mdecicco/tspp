#include <tspp/bind.h>
#include <tspp/builtin/process.h>
#include <tspp/utils/Docs.h>

#include <filesystem>
#include <v8.h>

using namespace bind;

namespace tspp::builtin::process {
    String getCwd() {
        return std::filesystem::current_path().string();
    }

    void init() {
        static String os = "win32";

        Namespace* ns = new Namespace("process");
        Registry::Add(ns);

        describe(ns->function("cwd", getCwd))
            .desc("Gets the current working directory")
            .returns("The current working directory", false);

        ns->value("os", &os);
    }
}
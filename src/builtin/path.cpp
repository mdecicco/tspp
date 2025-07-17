#include <tspp/builtin/path.h>
#include <tspp/bind.h>
#include <tspp/utils/Docs.h>

#include <utils/String.h>
#include <utils/Array.hpp>

#include <filesystem>

using namespace bind;

namespace tspp::builtin::path {
    bool isAbsolutePath(const String& path) {
        if (path.size() == 0) return false;
        if (path[0] == '/') return true;
        if (path[0] == '\\') return true;
        if (path.size() >= 2 && path[1] == ':' && isalpha(path[0])) return true;
        return false;
    }

    String normalize(const String& path) {
        if (path.size() == 0) return ".";
        
        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');

        size_t pos = 0;
        while ((pos = normalized.find("//", pos)) != std::string::npos) {
            normalized.erase(pos, 1);
        }

        Array<std::string> segments;
        size_t start = 0;
        bool isAbsolute = isAbsolutePath(path);

        while (start < normalized.size()) {
            size_t end = normalized.find('/', start);
            if (end == std::string::npos) end = normalized.size();

            std::string segment = normalized.substr(start, end - start);

            if (segment == "..") {
                if (segments.size() > 0 && segments.last() != "..") {
                    segments.pop();
                } else if (!isAbsolute) {
                    segments.push(segment);
                }
            } else if (segment != "." && segment.size() > 0) {
                segments.push(segment);
            }

            start = end + 1;
        }

        if (segments.size() == 0) return ".";

        String result;
        if (isAbsolute && path[0] == '/') {
            result += "/";
        } else if (path[0] != '.') {
            result += "./";
        }

        for (u32 i = 0;i < segments.size();i++) {
            result += segments[i];
            if (i < segments.size() - 1) {
                result += "/";
            }
        }

        return result;
    }

    String dirname(const String& path) {
        String normalized = normalize(path);
        u32 pos = UINT32_MAX;

        for (u32 i = 0;i < normalized.size();i++) {
            if (normalized[i] == '/') {
                pos = i;
            }
        }

        if (pos == UINT32_MAX) return ".";

        String result;
        result.copy(normalized.c_str(), pos);
        return result;
    }

    void init() {
        Namespace* ns = new Namespace("path");
        Registry::Add(ns);
        
        describe(ns->function("isAbsolute", isAbsolutePath))
            .desc("Checks if a path is an absolute path")
            .param(0, "path", "The path to check")
            .returns("True if the path is an absolute path, false otherwise");

        describe(ns->function("normalize", normalize))
            .desc("Normalizes a path")
            .param(0, "path", "The path to normalize")
            .returns("The normalized path");

        describe(ns->function("dirname", dirname))
            .desc("Gets the directory name of a path")
            .param(0, "path", "The path to get the directory name of")
            .returns("The directory name of the path");
    }
}
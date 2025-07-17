#include <tspp/utils/SourceFileBuilder.h>
#include <utils/Array.hpp>

#include <stdarg.h>

namespace tspp {
    SourceFileBuilder::SourceFileBuilder() {
        m_indentLevel = 0;
        m_needsIndent = true;
    }

    SourceFileBuilder::~SourceFileBuilder() {
    }

    void SourceFileBuilder::indent() {
        m_indentLevel++;
    }

    void SourceFileBuilder::unindent() {
        if (m_indentLevel == 0) return;
        m_indentLevel--;
    }

    void SourceFileBuilder::operator+=(const String& str) {
        Array<String> lines;
        String cur;
        bool hadNewLine = false;
        for (u32 i = 0;i < str.size();i++) {
            if (str[i] == '\n') {
                lines.push(cur);
                cur = "";
                hadNewLine = true;
            } else {
                cur += str[i];
            }
        }

        if (cur.size() > 0) {
            lines.push(cur);
        }

        for (u32 i = 0;i < lines.size();i++) {
            if (i > 0) newline();
            ensureIndent();
            m_content += lines[i];
        }

        if (lines.size() == 1 && hadNewLine) {
            newline();
        }
    }

    void SourceFileBuilder::newline() {
        m_content += "\n";
        m_needsIndent = true;
    }

    void SourceFileBuilder::line(const String& str) {
        (*this) += str;
        newline();
    }

    void SourceFileBuilder::line(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        (*this) += buffer;
        newline();
    }

    void SourceFileBuilder::ensureIndent() {
        if (!m_needsIndent) return;

        for (u32 i = 0;i < m_indentLevel;i++) {
            m_content += "    ";
        }

        m_needsIndent = false;
    }

    const String& SourceFileBuilder::getContent() const {
        return m_content;
    }

    bool SourceFileBuilder::writeToFile(const String& path) const {
        FILE* file = fopen(path.c_str(), "w");
        if (!file) return false;
        fwrite(m_content.c_str(), 1, m_content.size(), file);
        fclose(file);
        return true;
    }
}


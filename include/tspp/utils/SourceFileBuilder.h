#pragma once
#include <tspp/types.h>

#include <utils/String.h>

namespace tspp {
    class SourceFileBuilder {
        public:
            SourceFileBuilder();
            ~SourceFileBuilder();

            void indent();
            void unindent();
            void operator+=(const String& str);
            void newline();
            void line(const String& str);
            void line(const char* fmt, ...);

            const String& getContent() const;
            bool writeToFile(const String& path) const;

        protected:
            void ensureIndent();
            u32 m_indentLevel;
            bool m_needsIndent;
            String m_content;
    };
}

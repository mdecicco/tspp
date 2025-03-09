#include <tspp/interfaces/IWithLogging.h>

#include <utils/Array.hpp>

#include <cstdarg>

namespace tspp {
    IWithLogging::IWithLogging(const String& contextName) : m_contextName(contextName), m_hasErrors(false) {}

    IWithLogging::~IWithLogging() {
    }

    void IWithLogging::addLogHandler(ILogHandler* handler) {
        m_loggers.push(handler);
    }

    void IWithLogging::removeLogHandler(ILogHandler* handler) {
        i64 index = m_loggers.findIndex([handler](ILogHandler* h) {
            return h == handler;
        });

        if (index == -1) return;

        m_loggers.remove(index);
    }

    const Array<ILogHandler*>& IWithLogging::getLogHandlers() const {
        return m_loggers;
    }

    void IWithLogging::logDebug(const char* msg, ...) {
        m_msgBuf[0] = 0;

        va_list l;
        va_start(l, msg);
        u32 off = 0;
        if (m_contextName.size() > 0) off = snprintf(m_msgBuf, 4096, "[%s] ", m_contextName.c_str());
        vsnprintf(m_msgBuf + off, 4096 - off, msg, l);
        va_end(l);

        onDebug(m_msgBuf);
    }

    void IWithLogging::logInfo(const char* msg, ...) {
        m_msgBuf[0] = 0;

        va_list l;
        va_start(l, msg);
        u32 off = 0;
        if (m_contextName.size() > 0) off = snprintf(m_msgBuf, 4096, "[%s] ", m_contextName.c_str());
        vsnprintf(m_msgBuf + off, 4096 - off, msg, l);
        va_end(l);

        onInfo(m_msgBuf);
    }

    void IWithLogging::logWarn(const char* msg, ...) {
        m_msgBuf[0] = 0;

        va_list l;
        va_start(l, msg);
        u32 off = 0;
        if (m_contextName.size() > 0) off = snprintf(m_msgBuf, 4096, "[%s] ", m_contextName.c_str());
        vsnprintf(m_msgBuf + off, 4096 - off, msg, l);
        va_end(l);

        onWarn(m_msgBuf);
    }

    void IWithLogging::logError(const char* msg, ...) {
        m_hasErrors = true;
        
        m_msgBuf[0] = 0;

        va_list l;
        va_start(l, msg);
        u32 off = 0;
        if (m_contextName.size() > 0) off = snprintf(m_msgBuf, 4096, "[%s] ", m_contextName.c_str());
        vsnprintf(m_msgBuf + off, 4096 - off, msg, l);
        va_end(l);

        onError(m_msgBuf);
    }

    bool IWithLogging::didError() const {
        return m_hasErrors;
    }

    void IWithLogging::onDebug(const char* msg) {
        if (m_loggers.size() == 0) return;

        for (ILogHandler* handler : m_loggers) {
            handler->onDebug(msg);
        }
    }

    void IWithLogging::onInfo(const char* msg) {
        if (m_loggers.size() == 0) return;

        for (ILogHandler* handler : m_loggers) {
            handler->onInfo(msg);
        }
    }
    
    void IWithLogging::onWarn(const char* msg) {
        if (m_loggers.size() == 0) return;

        for (ILogHandler* handler : m_loggers) {
            handler->onWarn(msg);
        }
    }

    void IWithLogging::onError(const char* msg) {
        m_hasErrors = true;
        if (m_loggers.size() == 0) return;

        for (ILogHandler* handler : m_loggers) {
            handler->onError(msg);
        }
    }
}
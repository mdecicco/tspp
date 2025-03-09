#pragma once
#include <tspp/types.h>

#include <utils/interfaces/ILogHandler.h>
#include <utils/Array.h>
#include <utils/String.h>

namespace tspp {
    class IWithLogging : public ILogHandler {
        public:
            IWithLogging(const String& contextName = String());
            virtual ~IWithLogging();

            void addLogHandler(ILogHandler* handler);
            void removeLogHandler(ILogHandler* handler);
            const Array<ILogHandler*>& getLogHandlers() const;

            void logDebug(const char* msg, ...);
            void logInfo(const char* msg, ...);
            void logWarn(const char* msg, ...);
            void logError(const char* msg, ...);

            bool didError() const;
        
        private:
            char m_msgBuf[4096];
            String m_contextName;
            Array<ILogHandler*> m_loggers;
            bool m_hasErrors;
            
            virtual void onDebug(const char* msg);
            virtual void onInfo(const char* msg);
            virtual void onWarn(const char* msg);
            virtual void onError(const char* msg);
    };
}
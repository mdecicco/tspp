#pragma once
#include <tspp/types.h>

#include <utils/Array.h>
#include <utils/interfaces/IWithLogging.h>

#include <functional>
#include <ostream>
#include <thread>

namespace websocketpp {
    namespace config {
        struct asio;
    }

    template <typename config>
    class server;

    typedef std::weak_ptr<void> connection_hdl;
}

namespace tspp {
    class WebSocketServer;
    class WebSocketConnection;

    class SocketErrorStreamBuf : public std::streambuf {
        public:
            SocketErrorStreamBuf(WebSocketServer* socket);
            int overflow(int c) override;
            std::streamsize xsputn(const char* s, std::streamsize n) override;
            int sync() override;

        private:
            WebSocketServer* m_socket;
            String m_buffer;
    };

    class SocketErrorHandler : public std::ostream {
        public:
            SocketErrorHandler(WebSocketServer* socket);

        private:
            SocketErrorStreamBuf m_streamBuf;
    };

    class IWebSocketServerListener {
        public:
            virtual ~IWebSocketServerListener() = default;
            virtual void onMessage(WebSocketConnection* connection, const void* data, u64 size);
            virtual void onConnect(WebSocketConnection* connection);
            virtual void onDisconnect(WebSocketConnection* connection);
    };

    class IWebSocketConnectionListener {
        public:
            virtual ~IWebSocketConnectionListener() = default;
            virtual void onMessage(const void* data, u64 size);
            virtual void onDisconnect();
    };

    class WebSocketConnection : public IWebSocketServerListener {
        public:
            bool isOpen() const;
            void send(void* data, u64 size);
            void sendText(const void* data, u64 size);
            void close();

            void addListener(IWebSocketConnectionListener* listener);
            void removeListener(IWebSocketConnectionListener* listener);

        protected:
            friend class WebSocketServer;
            WebSocketConnection(WebSocketServer* server, websocketpp::connection_hdl connection);
            ~WebSocketConnection();

            void onMessage(WebSocketConnection* connection, const void* data, u64 size) override;
            void onDisconnect(WebSocketConnection* connection) override;

            WebSocketServer* m_server;
            websocketpp::connection_hdl m_connection;
            Array<IWebSocketConnectionListener*> m_listeners;
    };

    class WebSocketServer : public IWithLogging {
        public:
            using HttpHandler = std::function<void(websocketpp::connection_hdl)>;
            using Server      = websocketpp::server<websocketpp::config::asio>;

            WebSocketServer(u16 port);
            ~WebSocketServer();

            void open();
            void close();
            bool isOpen() const;
            void broadcast(void* data, u64 size);
            void broadcastText(const void* data, u64 size);
            void processEvents();

            void setHttpHandler(const HttpHandler& handler);

            void addListener(IWebSocketServerListener* listener);
            void removeListener(IWebSocketServerListener* listener);
            const Array<WebSocketConnection*>& getConnections() const;
            Server* getServer() const;

        protected:
            Server* m_server;
            std::thread::id m_startedByThread;
            bool m_shouldStop;
            bool m_isProcessingEvents;
            u16 m_port;
            Array<WebSocketConnection*> m_connections;
            Array<IWebSocketServerListener*> m_listeners;
            SocketErrorHandler* m_errorHandler;
            HttpHandler m_httpHandler;
    };
}
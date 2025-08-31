#define TSPP_INCLUDING_WINDOWS_H
#include <tspp/builtin/socket_server.h>

#include <utils/Array.hpp>
#include <utils/Exception.h>

#define ASIO_STANDALONE
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

namespace tspp {
    //////////////////////////////////////////////////////////////////////////
    // SocketErrorStreamBuf
    //////////////////////////////////////////////////////////////////////////

    SocketErrorStreamBuf::SocketErrorStreamBuf(WebSocketServer* socket) {
        m_socket = socket;
    }

    int SocketErrorStreamBuf::overflow(int c) {
        if (c == std::streambuf::traits_type::eof() || c == '\n' || c == '\r') {
            return c;
        }

        m_buffer += char(c);
        return c;
    }

    std::streamsize SocketErrorStreamBuf::xsputn(const char* s, std::streamsize n) {
        for (i32 i = 0; i < n; i++) {
            if (s[i] == '\n' || s[i] == '\r') {
                continue;
            }

            m_buffer += s[i];
        }

        return n;
    }

    int SocketErrorStreamBuf::sync() {
        i32 len = m_buffer.size();
        if (len > 0) {
            m_socket->error("%s", m_buffer.c_str());
        }

        m_buffer = "";

        return len;
    }

    //////////////////////////////////////////////////////////////////////////
    // SocketErrorHandler
    //////////////////////////////////////////////////////////////////////////

    SocketErrorHandler::SocketErrorHandler(WebSocketServer* socket) : std::ostream(&m_streamBuf), m_streamBuf(socket) {}

    //////////////////////////////////////////////////////////////////////////
    // IWebSocketServerListener
    //////////////////////////////////////////////////////////////////////////

    void IWebSocketServerListener::onMessage(WebSocketConnection* connection, const void* data, u64 size) {}

    void IWebSocketServerListener::onConnect(WebSocketConnection* connection) {}

    void IWebSocketServerListener::onDisconnect(WebSocketConnection* connection) {}

    //////////////////////////////////////////////////////////////////////////
    // IWebSocketConnectionListener
    //////////////////////////////////////////////////////////////////////////

    void IWebSocketConnectionListener::onMessage(const void* data, u64 size) {}

    void IWebSocketConnectionListener::onDisconnect() {}

    //////////////////////////////////////////////////////////////////////////
    // WebSocketConnection
    //////////////////////////////////////////////////////////////////////////

    WebSocketConnection::WebSocketConnection(WebSocketServer* server, websocketpp::connection_hdl connection) {
        m_server     = server;
        m_connection = connection;
    }

    WebSocketConnection::~WebSocketConnection() {}

    bool WebSocketConnection::isOpen() const {
        using connection_type       = websocketpp::connection<websocketpp::config::asio>;
        connection_type* sharedConn = (connection_type*)m_connection.lock().get();
        return sharedConn->get_state() == websocketpp::session::state::open;
    }

    void WebSocketConnection::send(void* data, u64 size) {
        using connection_type       = websocketpp::connection<websocketpp::config::asio>;
        connection_type* sharedConn = (connection_type*)m_connection.lock().get();
        sharedConn->send(data, size, websocketpp::frame::opcode::binary);
    }

    void WebSocketConnection::sendText(const void* data, u64 size) {
        using connection_type       = websocketpp::connection<websocketpp::config::asio>;
        connection_type* sharedConn = (connection_type*)m_connection.lock().get();
        sharedConn->send(data, size, websocketpp::frame::opcode::text);
    }

    void WebSocketConnection::close() {
        using connection_type       = websocketpp::connection<websocketpp::config::asio>;
        connection_type* sharedConn = (connection_type*)m_connection.lock().get();
        sharedConn->close(websocketpp::close::status::normal, "Socket closed");
    }

    void WebSocketConnection::addListener(IWebSocketConnectionListener* listener) {
        m_listeners.push(listener);
    }

    void WebSocketConnection::removeListener(IWebSocketConnectionListener* listener) {
        for (u32 i = 0; i < m_listeners.size(); i++) {
            if (m_listeners[i] == listener) {
                m_listeners.remove(i);
                break;
            }
        }
    }

    void WebSocketConnection::onMessage(WebSocketConnection* connection, const void* data, u64 size) {
        if (connection != this) {
            return;
        }

        for (u32 i = 0; i < m_listeners.size(); i++) {
            m_listeners[i]->onMessage(data, size);
        }
    }

    void WebSocketConnection::onDisconnect(WebSocketConnection* connection) {
        if (connection != this) {
            return;
        }

        for (u32 i = 0; i < m_listeners.size(); i++) {
            m_listeners[i]->onDisconnect();
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // WebSocketServer
    //////////////////////////////////////////////////////////////////////////

    WebSocketServer::WebSocketServer(u16 port) : IWithLogging("Socket") {
        m_port               = port;
        m_shouldStop         = false;
        m_isProcessingEvents = false;
        m_errorHandler       = new SocketErrorHandler(this);
        m_server             = new Server();
    }

    WebSocketServer::~WebSocketServer() {
        close();
        delete m_server;
        delete m_errorHandler;
    }

    void WebSocketServer::open() {
        if (isOpen()) {
            throw InvalidActionException("WebSocketServer::open() - Socket is already open");
        }

        m_server->set_access_channels(websocketpp::log::alevel::none);
        m_server->set_error_channels(websocketpp::log::elevel::all);
        m_server->get_elog().set_ostream(m_errorHandler);

        m_server->set_message_handler([this](websocketpp::connection_hdl hdl, Server::message_ptr msg) {
            WebSocketConnection* connection = nullptr;
            for (u32 i = 0; i < m_connections.size(); i++) {
                if (m_connections[i]->m_connection.lock().get() == hdl.lock().get()) {
                    connection = m_connections[i];
                    break;
                }
            }

            if (!connection) {
                error("WebSocketServer::onMessage() - Connection not found");
                return;
            }

            for (u32 i = 0; i < m_listeners.size(); i++) {
                m_listeners[i]->onMessage(connection, msg->get_payload().data(), msg->get_payload().size());
            }
        });

        m_server->set_open_handler([this](websocketpp::connection_hdl hdl) {
            WebSocketConnection* connection = new WebSocketConnection(this, hdl);
            m_connections.push(connection);

            for (u32 i = 0; i < m_listeners.size(); i++) {
                m_listeners[i]->onConnect(connection);
            }

            addListener(connection);
        });

        m_server->set_close_handler([this](websocketpp::connection_hdl hdl) {
            for (u32 i = 0; i < m_connections.size(); i++) {
                try {
                    if (m_connections[i]->m_connection.lock().get() == hdl.lock().get()) {
                        for (u32 j = 0; j < m_listeners.size(); j++) {
                            m_listeners[j]->onDisconnect(m_connections[i]);
                        }

                        removeListener(m_connections[i]);
                        delete m_connections[i];
                        m_connections.remove(i);
                        break;
                    }
                } catch (const std::exception& e) {
                    continue;
                }
            }
        });

        // Optional HTTP handler (for inspector /json endpoints, etc.)
        if (m_httpHandler) {
            m_server->set_http_handler(m_httpHandler);
        }

        m_server->init_asio();
        m_server->listen(m_port);
        m_server->start_accept();
        m_startedByThread = std::this_thread::get_id();

        log("WebSocketServer::open() - Socket server started on port %d", m_port);
    }

    void WebSocketServer::close() {
        if (!isOpen()) {
            return;
        }

        // if this is not the main thread, just raise the shouldStop flag
        if (m_isProcessingEvents || std::this_thread::get_id() != m_startedByThread) {
            m_shouldStop = true;
            return;
        }

        for (u32 i = 0; i < m_connections.size(); i++) {
            m_connections[i]->close();
        }

        m_server->stop_listening();
        m_server->stop();
        m_connections.clear();

        log("WebSocketServer::close() - Socket server closed");
    }

    bool WebSocketServer::isOpen() const {
        return m_server->is_listening();
    }

    void WebSocketServer::broadcast(void* data, u64 size) {
        for (u32 i = 0; i < m_connections.size(); i++) {
            m_connections[i]->send(data, size);
        }
    }

    void WebSocketServer::broadcastText(const void* data, u64 size) {
        for (u32 i = 0; i < m_connections.size(); i++) {
            m_connections[i]->sendText(data, size);
        }
    }

    void WebSocketServer::processEvents() {
        if (m_shouldStop) {
            close();
            return;
        }

        m_isProcessingEvents = true;
        m_server->poll_one();
        m_isProcessingEvents = false;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // setHttpHandler
    ////////////////////////////////////////////////////////////////////////////////

    void WebSocketServer::setHttpHandler(const HttpHandler& handler) {
        m_httpHandler = handler;
    }

    void WebSocketServer::addListener(IWebSocketServerListener* listener) {
        m_listeners.push(listener);
    }

    void WebSocketServer::removeListener(IWebSocketServerListener* listener) {
        for (u32 i = 0; i < m_listeners.size(); i++) {
            if (m_listeners[i] == listener) {
                m_listeners.remove(i);
                break;
            }
        }
    }

    const Array<WebSocketConnection*>& WebSocketServer::getConnections() const {
        return m_connections;
    }

    WebSocketServer::Server* WebSocketServer::getServer() const {
        return m_server;
    }
}

#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>

class QTcpServer;
class QTcpSocket;

namespace sony::devicecenter {

class DeviceCenterController;

// Minimal HTTP/1.1 API on 127.0.0.1 so local tools (the Stream Deck plugin)
// can read state and drive the headset through the app, which owns the only
// Bluetooth control channel. See docs/local-api.md.
//
// It never listens beyond loopback. Requests whose Host is not a loopback name
// are refused (DNS rebinding), and state-changing POSTs need the
// X-Sony-Device-Center header, which a web page cannot add cross-origin
// without a CORS preflight this server never approves.
class LocalApi : public QObject {
    Q_OBJECT
public:
    static constexpr quint16 kPort = 47821;

    explicit LocalApi(DeviceCenterController* controller, QObject* parent = nullptr);
    ~LocalApi() override;

private:
    struct Response {
        int status{200};
        QByteArray body;
    };

    void _applyEnabled();
    void _onConnection();
    void _onReadyRead(QTcpSocket* socket);
    Response _handle(const QByteArray& method, const QByteArray& path, const QHash<QByteArray, QByteArray>& headers);
    QByteArray _status() const;
    static void _reply(QTcpSocket* socket, const Response& response);

    DeviceCenterController* _controller;
    QTcpServer* _server{nullptr};
    QHash<QTcpSocket*, QByteArray> _buffers;
};

} // namespace sony::devicecenter

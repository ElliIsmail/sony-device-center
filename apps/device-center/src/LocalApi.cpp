#include "LocalApi.h"
#include "DeviceCenterController.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>

namespace sony::devicecenter {

namespace {

constexpr qsizetype kMaxRequestBytes = 8 * 1024;

QByteArray error(const char* message) {
    return QJsonDocument(QJsonObject{{"ok", false}, {"error", message}}).toJson(QJsonDocument::Compact);
}

QByteArray ok() {
    return QJsonDocument(QJsonObject{{"ok", true}}).toJson(QJsonDocument::Compact);
}

bool isLoopbackHost(QByteArray host) {
    // Strip the port, keeping bracketed IPv6 literals intact.
    if (host.startsWith('[')) {
        host = host.mid(1, host.indexOf(']') - 1);
    } else if (const auto colon = host.lastIndexOf(':'); colon >= 0) {
        host = host.left(colon);
    }
    host = host.toLower();
    return host == "127.0.0.1" || host == "localhost" || host == "::1";
}

const char* reasonPhrase(int status) {
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    default:  return "Error";
    }
}

} // namespace

LocalApi::LocalApi(DeviceCenterController* controller, QObject* parent)
    : QObject(parent), _controller(controller) {
    connect(_controller, &DeviceCenterController::localApiEnabledChanged, this, &LocalApi::_applyEnabled);
    _applyEnabled();
}

LocalApi::~LocalApi() = default;

void LocalApi::_applyEnabled() {
    const bool enabled = _controller->localApiEnabled();
    if (enabled && !_server) {
        _server = new QTcpServer(this);
        connect(_server, &QTcpServer::newConnection, this, &LocalApi::_onConnection);
        if (!_server->listen(QHostAddress::LocalHost, kPort)) {
            qWarning("Local API: cannot listen on 127.0.0.1:%u: %s", kPort, qPrintable(_server->errorString()));
            delete _server;
            _server = nullptr;
        }
    } else if (!enabled && _server) {
        _server->close();
        delete _server;
        _server = nullptr;
    }
}

void LocalApi::_onConnection() {
    while (auto* socket = _server->nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { _onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            _buffers.remove(socket);
            socket->deleteLater();
        });
    }
}

void LocalApi::_onReadyRead(QTcpSocket* socket) {
    QByteArray& buffer = _buffers[socket];
    buffer += socket->readAll();
    if (buffer.size() > kMaxRequestBytes) {
        _reply(socket, {413, error("request too large")});
        return;
    }
    const auto headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) return; // wait for the rest of the head

    // No endpoint takes a body, so everything after the head is ignored.
    const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
    const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
    if (requestLine.size() != 3 || !requestLine[2].startsWith("HTTP/1.")) {
        _reply(socket, {400, error("malformed request")});
        return;
    }
    QHash<QByteArray, QByteArray> headers;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const auto colon = lines[i].indexOf(':');
        if (colon > 0) headers.insert(lines[i].left(colon).trimmed().toLower(), lines[i].mid(colon + 1).trimmed());
    }
    QByteArray path = requestLine[1];
    if (const auto query = path.indexOf('?'); query >= 0) path.truncate(query);

    const Response response = _handle(requestLine[0], path, headers);
    _buffers.remove(socket);
    _reply(socket, response);
}

LocalApi::Response LocalApi::_handle(const QByteArray& method, const QByteArray& path,
                                     const QHash<QByteArray, QByteArray>& headers) {
    if (!isLoopbackHost(headers.value("host")))
        return {403, error("host not allowed")};

    if (path == "/status") {
        if (method != "GET") return {405, error("use GET")};
        return {200, _status()};
    }

    const bool isAction = path.startsWith("/noise-control/") || path.startsWith("/speak-to-chat/");
    if (!isAction) return {404, error("not found")};
    if (method != "POST") return {405, error("use POST")};
    if (headers.value("x-sony-device-center") != "1")
        return {403, error("missing X-Sony-Device-Center: 1 header")};
    if (!_controller->isConnected()) return {409, error("headphones are not connected")};
    // The controller drops commands while one is in flight; say so rather than claim success.
    if (_controller->busy()) return {409, error("busy, retry shortly")};

    const QByteArray arg = path.mid(path.lastIndexOf('/') + 1);
    if (path.startsWith("/noise-control/")) {
        QByteArray mode = arg;
        if (mode == "next") {
            // Same order as the overview quick actions, skipping what the model lacks.
            const QString current = _controller->noiseControlMode();
            mode = current == "cancelling" ? (_controller->hasAmbient() ? "ambient" : "off")
                 : current == "ambient"    ? "off"
                 : (_controller->hasAnc() ? "cancelling" : "ambient");
        }
        if (mode == "cancelling" && _controller->hasAnc()) _controller->setAnc(true);
        else if (mode == "ambient" && _controller->hasAmbient())
            _controller->setAmbient(_controller->ambientLevel(), _controller->focusOnVoice());
        else if (mode == "off") _controller->setNoiseControlOff();
        else return {400, error("mode must be cancelling, ambient, off or next")};
        return {200, ok()};
    }

    if (!_controller->hasSpeakToChat()) return {409, error("Speak-to-Chat is not supported by this device")};
    if (arg == "on") _controller->setSpeakToChat(true);
    else if (arg == "off") _controller->setSpeakToChat(false);
    else if (arg == "toggle") _controller->setSpeakToChat(!_controller->speakToChat());
    else return {400, error("use on, off or toggle")};
    return {200, ok()};
}

QByteArray LocalApi::_status() const {
    const bool connected = _controller->isConnected();
    const int battery = _controller->batteryLevel();
    QJsonObject status{
        {"ok", true},
        {"connected", connected},
        {"device", _controller->deviceName()},
        {"battery", connected && battery >= 0 ? QJsonValue(battery) : QJsonValue()},
        {"charging", connected && _controller->isCharging()},
        {"noiseControl", connected ? _controller->noiseControlMode() : QStringLiteral("unknown")},
        {"ambientLevel", _controller->ambientLevel()},
        {"speakToChat", _controller->hasSpeakToChat() ? QJsonValue(_controller->speakToChat()) : QJsonValue()},
    };
    return QJsonDocument(status).toJson(QJsonDocument::Compact);
}

void LocalApi::_reply(QTcpSocket* socket, const Response& response) {
    // One request per connection: ignore anything the client sends after it.
    QObject::disconnect(socket, &QTcpSocket::readyRead, nullptr, nullptr);
    QByteArray out = "HTTP/1.1 " + QByteArray::number(response.status) + ' ' + reasonPhrase(response.status) + "\r\n";
    out += "Content-Type: application/json\r\n";
    out += "Cache-Control: no-store\r\n";
    out += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n";
    out += "Connection: close\r\n\r\n";
    out += response.body;
    socket->write(out);
    socket->disconnectFromHost();
}

} // namespace sony::devicecenter

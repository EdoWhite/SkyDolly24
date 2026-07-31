/**
 * Sky Dolly - The Black Sheep for Your Flight Recordings
 *
 * Copyright (c) 2020 - 2025 Oliver Knoll
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <QObject>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

#include <PluginManager/SkyConnectManager.h>

#include "ApiHandler.h"
#include "HttpRequest.h"
#include "RemoteServer.h"

namespace
{
    const char *statusText(int statusCode) noexcept
    {
        switch (statusCode) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 503: return "Service Unavailable";
        default: return "Internal Server Error";
        }
    }

    // The panel is loaded from a coui:// origin, which Coherent GT presents as an opaque origin.
    // There is no origin to allow-list, so every response has to be permissive. This is only
    // acceptable because the listening socket is unreachable from outside this machine.
    QByteArray commonHeaders() noexcept
    {
        return QByteArrayLiteral(
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Headers: Content-Type, X-Sky-Dolly-Token\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Cache-Control: no-store\r\n");
    }

    bool isLoopback(const QHostAddress &address) noexcept
    {
        // Covers 127.0.0.0/8 and ::1, and the IPv4-mapped form ::ffff:127.0.0.1 that a dual stack
        // listener reports
        return address.isLoopback() ||
               QHostAddress(address.toIPv4Address()).isLoopback();
    }
}

namespace
{
    // The panel's timeline has to advance during replay, but the timestamp changes once per
    // simulator frame: pushing that would send some 60 messages a second into the simulator's
    // browser for no visible benefit. Push at a rate that looks continuous to the eye instead.
    constexpr int StateBroadcastIntervalMSec {250};
}

struct RemoteServerPrivate
{
    QTcpServer tcpServer;
    ApiHandler apiHandler;
    QTimer broadcastTimer;
    QString lastError;
    quint16 port {0};
    // Buffered bytes per open connection: a request may arrive in several segments
    std::unordered_map<QTcpSocket *, QByteArray> buffers;
    // Connections upgraded to a Server-Sent Events stream, which stay open and are written to
    // whenever the application state changes
    std::unordered_set<QTcpSocket *> eventStreams;
};

// PUBLIC

RemoteServer::RemoteServer(QObject *parent) noexcept
    : QObject {parent},
      d {std::make_unique<RemoteServerPrivate>()}
{
    frenchConnection();
}

RemoteServer::~RemoteServer()
{
    stop();
}

bool RemoteServer::start(quint16 port) noexcept
{
    if (d->tcpServer.isListening()) {
        stop();
    }
    if (!d->tcpServer.listen(QHostAddress::LocalHost, port)) {
        d->lastError = d->tcpServer.errorString();
        qWarning() << "RemoteServer: could not listen on 127.0.0.1:" << port << "-" << d->lastError;
        return false;
    }
    d->port = d->tcpServer.serverPort();
    d->lastError.clear();
    qInfo() << "RemoteServer: listening on 127.0.0.1:" << d->port;
    return true;
}

void RemoteServer::stop() noexcept
{
    for (const auto &[socket, buffer] : d->buffers) {
        socket->disconnect(this);
        socket->close();
        socket->deleteLater();
    }
    d->buffers.clear();
    d->eventStreams.clear();
    d->broadcastTimer.stop();
    if (d->tcpServer.isListening()) {
        d->tcpServer.close();
        qInfo() << "RemoteServer: stopped";
    }
    d->port = 0;
}

bool RemoteServer::isListening() const noexcept
{
    return d->tcpServer.isListening();
}

quint16 RemoteServer::getPort() const noexcept
{
    return d->port;
}

QString RemoteServer::getLastError() const noexcept
{
    return d->lastError;
}

void RemoteServer::setAccessToken(const QString &accessToken) noexcept
{
    d->apiHandler.setAccessToken(accessToken);
}

// PRIVATE

void RemoteServer::frenchConnection() noexcept
{
    connect(&d->tcpServer, &QTcpServer::newConnection,
            this, &RemoteServer::handleNewConnection);

    // The panel mirrors the application, so anything that changes what the panel shows has to
    // reach it without polling
    auto &skyConnectManager = SkyConnectManager::getInstance();
    connect(&skyConnectManager, &SkyConnectManager::stateChanged,
            this, [this]() noexcept { broadcastState(); });
    connect(&skyConnectManager, &SkyConnectManager::connectionChanged,
            this, [this]() noexcept { broadcastState(); });

    // While a replay or recording is running the timestamp moves continuously; the timer is what
    // keeps the panel's timeline in step with it. It only runs while somebody is listening.
    d->broadcastTimer.setInterval(::StateBroadcastIntervalMSec);
    connect(&d->broadcastTimer, &QTimer::timeout, this, [this]() noexcept {
        if (SkyConnectManager::getInstance().isActive()) {
            broadcastState();
        }
    });
}

void RemoteServer::updateBroadcastTimer() noexcept
{
    if (d->eventStreams.empty()) {
        d->broadcastTimer.stop();
    } else if (!d->broadcastTimer.isActive()) {
        d->broadcastTimer.start();
    }
}

void RemoteServer::handleNewConnection() noexcept
{
    while (QTcpSocket *socket = d->tcpServer.nextPendingConnection()) {
        if (!::isLoopback(socket->peerAddress())) {
            // Defence in depth: the listener is already bound to loopback, but a misconfiguration
            // there must not turn into remote control of the simulator
            qWarning() << "RemoteServer: refused a connection from" << socket->peerAddress().toString();
            socket->abort();
            socket->deleteLater();
            continue;
        }

        d->buffers[socket] = {};
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() noexcept {
            handleReadyRead(socket);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() noexcept {
            closeSocket(socket);
        });
    }
}

void RemoteServer::handleReadyRead(QTcpSocket *socket) noexcept
{
    const auto bufferIt = d->buffers.find(socket);
    if (bufferIt == d->buffers.end()) {
        return;
    }
    QByteArray &buffer = bufferIt->second;
    buffer.append(socket->readAll());

    while (true) {
        HttpRequest request;
        int consumed {0};
        const HttpRequest::Result result = HttpRequest::parse(buffer, request, consumed);
        if (result == HttpRequest::Result::Incomplete) {
            if (buffer.size() > HttpRequest::MaxRequestSize) {
                closeSocket(socket);
            }
            return;
        }
        if (result == HttpRequest::Result::Malformed) {
            closeSocket(socket);
            return;
        }
        buffer.remove(0, consumed);

        const ApiHandler::Response response = d->apiHandler.handle(request);

        if (response.isEventStream) {
            // Keep the connection open and stream state changes over it. No Content-Length: the
            // body ends when the connection does.
            socket->write("HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/event-stream\r\n"
                          "Connection: keep-alive\r\n");
            socket->write(::commonHeaders());
            socket->write("\r\n");
            d->eventStreams.insert(socket);
            updateBroadcastTimer();
            // Send the current state straight away, so the panel paints something before the
            // first change rather than staying blank
            const QByteArray state = QJsonDocument(d->apiHandler.getState()).toJson(QJsonDocument::Compact);
            socket->write("data: " + state + "\r\n\r\n");
            socket->flush();
            return;
        }

        QByteArray header = "HTTP/1.1 " + QByteArray::number(response.statusCode) + " " +
                            ::statusText(response.statusCode) + "\r\n";
        header += "Content-Type: " + response.contentType + "\r\n";
        header += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n";
        header += "Connection: keep-alive\r\n";
        header += ::commonHeaders();
        header += "\r\n";
        socket->write(header);
        if (request.method != "HEAD") {
            socket->write(response.body);
        }
        socket->flush();
    }
}

void RemoteServer::closeSocket(QTcpSocket *socket) noexcept
{
    d->eventStreams.erase(socket);
    d->buffers.erase(socket);
    socket->disconnect(this);
    socket->close();
    socket->deleteLater();
    updateBroadcastTimer();
}

void RemoteServer::broadcastState() noexcept
{
    if (d->eventStreams.empty()) {
        return;
    }
    const QByteArray state = QJsonDocument(d->apiHandler.getState()).toJson(QJsonDocument::Compact);
    const QByteArray message = "data: " + state + "\r\n\r\n";
    // Copy first: writing to a broken stream disconnects it, which mutates the set
    const std::unordered_set<QTcpSocket *> streams = d->eventStreams;
    for (QTcpSocket *socket : streams) {
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->write(message);
            socket->flush();
        } else {
            closeSocket(socket);
        }
    }
}

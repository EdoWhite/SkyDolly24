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
#ifndef REMOTESERVER_H
#define REMOTESERVER_H

#include <memory>

#include <QObject>
#include <QString>

#include "RemoteLib.h"

class QTcpSocket;
struct RemoteServerPrivate;

/*!
 * A minimal HTTP/1.1 server that lets the in-game panel drive Sky Dolly.
 *
 * The simulator's panel runs inside Coherent GT and can only reach the outside world over HTTP, so
 * this is the bridge between the panel and the application. It is deliberately confined:
 *
 * - it binds to the loopback interface only, and additionally refuses any connection whose peer
 *   address is not loopback, so nothing on the network can reach it even if the bind were widened
 *   by mistake;
 * - it serves a fixed set of endpoints under /api and no files at all;
 * - it runs in the application's own event loop rather than a thread of its own, so handlers touch
 *   the model directly, with no locking and no chance of a half-applied command.
 *
 * An optional access token can be required on top of that, for the case where other software on
 * the same machine is not trusted.
 *
 * \sa ApiHandler
 */
class REMOTE_API RemoteServer : public QObject
{
    Q_OBJECT
public:
    explicit RemoteServer(QObject *parent = nullptr) noexcept;
    RemoteServer(const RemoteServer &rhs) = delete;
    RemoteServer(RemoteServer &&rhs) = delete;
    RemoteServer &operator=(const RemoteServer &rhs) = delete;
    RemoteServer &operator=(RemoteServer &&rhs) = delete;
    ~RemoteServer() override;

    /*! The port the panel expects, unless configured otherwise. */
    static constexpr quint16 DefaultPort {17285};

    /*!
     * Starts listening on the loopback interface.
     *
     * \param port
     *        the TCP port to listen on
     * \return \c true if the server is listening; \c false if the port could not be bound, in
     *         which case \c getLastError describes why
     */
    bool start(quint16 port = DefaultPort) noexcept;

    /*!
     * Stops listening and closes all open connections, including event streams.
     */
    void stop() noexcept;

    bool isListening() const noexcept;
    quint16 getPort() const noexcept;
    QString getLastError() const noexcept;

    /*!
     * Requires every request to carry \p accessToken in the X-Sky-Dolly-Token header. An empty
     * token - the default - disables the check.
     */
    void setAccessToken(const QString &accessToken) noexcept;

private:
    std::unique_ptr<RemoteServerPrivate> d;

    void frenchConnection() noexcept;
    void handleNewConnection() noexcept;
    void handleReadyRead(QTcpSocket *socket) noexcept;
    void closeSocket(QTcpSocket *socket) noexcept;
    void broadcastState() noexcept;
    void updateBroadcastTimer() noexcept;
};

#endif // REMOTESERVER_H

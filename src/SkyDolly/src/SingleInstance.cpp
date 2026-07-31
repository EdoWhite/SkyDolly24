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

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>
#include <QStringBuilder>
#include <QDir>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDebug>

#include <Kernel/Version.h>

#include "SingleInstance.h"

namespace
{
    // How long the second process waits for the first one: long enough for a busy machine that is
    // also running a flight simulator, short enough not to look like a hang
    constexpr int ConnectTimeoutMSec {1000};
    constexpr int WriteTimeoutMSec {1000};
}

// PUBLIC

SingleInstance::SingleInstance(QObject *parent) noexcept
    : QObject {parent}
{}

SingleInstance::~SingleInstance() = default;

bool SingleInstance::tryBecomePrimary(const QString &filePath) noexcept
{
    const QString serverName = getServerName();

    if (notifyRunningInstance(filePath)) {
        return false;
    }

    m_localServer = std::make_unique<QLocalServer>();
    // A process killed rather than closed leaves its socket behind; without this the surviving
    // stale name would keep every future launch from ever becoming primary
    QLocalServer::removeServer(serverName);
    if (!m_localServer->listen(serverName)) {
        // Not being able to listen is not a reason to refuse to start: the worst case is that a
        // second instance is not detected, which is how the application behaved until now
        qWarning() << "SingleInstance: could not listen on" << serverName
                   << "-" << m_localServer->errorString()
                   << "- a second instance will not be detected";
        m_localServer.reset();
        return true;
    }

    connect(m_localServer.get(), &QLocalServer::newConnection,
            this, &SingleInstance::handleNewConnection);
    return true;
}

// PRIVATE

QString SingleInstance::getServerName() noexcept
{
    // Per user, not per machine: two users on the same machine have separate logbooks and must be
    // able to run Sky Dolly at the same time. The home directory hash keeps the name short and
    // free of characters a local socket name cannot carry.
    const QByteArray userKey = QCryptographicHash::hash(QDir::homePath().toUtf8(),
                                                        QCryptographicHash::Sha1).toHex().left(12);
    return Version::getApplicationName() % QStringLiteral("-") % QString::fromLatin1(userKey);
}

bool SingleInstance::notifyRunningInstance(const QString &filePath) noexcept
{
    QLocalSocket socket;
    socket.connectToServer(getServerName());
    if (!socket.waitForConnected(::ConnectTimeoutMSec)) {
        // Nobody listening: either no instance is running, or one is starting up at the same
        // moment. The latter is a race this deliberately does not try to win - losing it means two
        // instances, which is what happened before this class existed.
        return false;
    }

    // Always terminated by a newline, and never empty: writing nothing at all - which is what an
    // absent logbook path would amount to - never raises readyRead on the other side, so the
    // running instance would not learn that it should come to the front.
    socket.write(filePath.toUtf8() + '\n');
    socket.flush();
    socket.waitForBytesWritten(::WriteTimeoutMSec);
    socket.disconnectFromServer();
    qInfo() << "SingleInstance: another instance is already running, handing over and exiting";
    return true;
}

void SingleInstance::handleNewConnection() noexcept
{
    QLocalSocket *socket = m_localServer->nextPendingConnection();
    if (socket == nullptr) {
        return;
    }
    connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    connect(socket, &QLocalSocket::readyRead, this, [this, socket]() noexcept {
        if (!socket->canReadLine()) {
            // The message may arrive in pieces; it is complete at the newline
            return;
        }
        const QString filePath = QString::fromUtf8(socket->readLine()).trimmed();
        emit anotherInstanceStarted(filePath);
        socket->disconnectFromServer();
    });
}

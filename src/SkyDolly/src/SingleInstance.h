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
#ifndef SINGLEINSTANCE_H
#define SINGLEINSTANCE_H

#include <memory>

#include <QObject>
#include <QString>

class QLocalServer;

/*!
 * Ensures that only one Sky Dolly runs at a time, and hands any further launch over to it.
 *
 * This became necessary with the simulator starting Sky Dolly by itself: the user launching the
 * desktop shortcut while the simulator has already started the service is no longer unlikely but
 * routine, and two processes opening the same SQLite logbook is a good way to lose a recording.
 *
 * The first process listens on a local socket named after the application. A second process finds
 * that socket, sends its command line to the first - which brings its window up - and exits.
 */
class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(QObject *parent = nullptr) noexcept;
    ~SingleInstance() override;

    /*!
     * Tries to become the one running instance.
     *
     * \param filePath
     *        the logbook this process was asked to open, empty if none; handed to the already
     *        running instance when there is one. Pass the parsed path rather than a raw argument:
     *        the command line also carries options.
     * \return \c true if this process is the first one and should carry on starting up; \c false
     *         if another instance is already running, has been asked to come to the front, and
     *         this process should exit
     */
    bool tryBecomePrimary(const QString &filePath) noexcept;

signals:
    /*!
     * Emitted when a second process was started and handed over to this one.
     *
     * \param filePath
     *        the logbook the second process was asked to open, empty if none
     */
    void anotherInstanceStarted(const QString &filePath);

private:
    std::unique_ptr<QLocalServer> m_localServer;

    static QString getServerName() noexcept;
    bool notifyRunningInstance(const QString &filePath) noexcept;
    void handleNewConnection() noexcept;
};

#endif // SINGLEINSTANCE_H

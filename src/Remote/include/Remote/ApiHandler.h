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
#ifndef APIHANDLER_H
#define APIHANDLER_H

#include <memory>

#include <QByteArray>
#include <QString>
#include <QJsonObject>

#include "HttpRequest.h"
#include "RemoteLib.h"

struct ApiHandlerPrivate;

/*!
 * Turns HTTP requests into calls on the application facades, and application state into JSON.
 *
 * Everything reachable from here goes through SkyConnectManager and the persistence services -
 * the same facades the main window and the modules use - so the panel can never get at anything
 * the desktop user interface could not.
 *
 * The handler runs on the main thread, in the application's own event loop: the server it belongs
 * to is asynchronous rather than threaded, so no locking or queued invocation is involved.
 */
class REMOTE_API ApiHandler final
{
public:
    ApiHandler() noexcept;
    ApiHandler(const ApiHandler &rhs) = delete;
    ApiHandler(ApiHandler &&rhs) noexcept;
    ApiHandler &operator=(const ApiHandler &rhs) = delete;
    ApiHandler &operator=(ApiHandler &&rhs) noexcept;
    ~ApiHandler();

    /*!
     * A response to be written back to the client.
     */
    struct Response
    {
        int statusCode {200};
        QByteArray contentType {"application/json"};
        QByteArray body;
        /*!
         * When true the connection is not an ordinary request/response exchange but a
         * Server-Sent Events stream that stays open. The server keeps the socket and pushes
         * state updates over it.
         */
        bool isEventStream {false};
    };

    /*!
     * Handles \p request.
     *
     * \param request
     *        the request as received from the loopback interface
     * \return the response to write back
     */
    Response handle(const HttpRequest &request) noexcept;

    /*!
     * Returns the current application state, the same object that \c GET \c /api/state answers
     * with and that is pushed over the event stream.
     */
    QJsonObject getState() const noexcept;

    /*!
     * Returns whether \p request carries the access token, when one is configured.
     *
     * With no token configured every request from the loopback interface is accepted, which is
     * the default: the panel has no way of reading a token out of the file system.
     */
    bool isAuthorised(const HttpRequest &request) const noexcept;

    /*!
     * Sets the token that requests must carry in the \c X-Sky-Dolly-Token header. An empty token
     * disables the check.
     */
    void setAccessToken(QString accessToken) noexcept;

private:
    std::unique_ptr<ApiHandlerPrivate> d;

    Response handleState() const noexcept;
    Response handleCommand(const HttpRequest &request) noexcept;
    Response handleFlights(const HttpRequest &request) const noexcept;
    Response handleLoad(const HttpRequest &request) noexcept;
    /*!
     * Serves the in-game panel itself, embedded as a resource, so that the same interface is
     * reachable from an ordinary browser on this machine. Returns 404 for anything that is not one
     * of the handful of embedded files: this is not a file server.
     */
    Response handlePanel(const QString &path) const noexcept;

    static Response makeJson(const QJsonObject &object, int statusCode = 200) noexcept;
    static Response makeError(int statusCode, const QString &message) noexcept;
};

#endif // APIHANDLER_H

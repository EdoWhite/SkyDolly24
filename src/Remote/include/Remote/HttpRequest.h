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
#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <optional>

#include <QByteArray>
#include <QString>
#include <QHash>

#include "RemoteLib.h"

/*!
 * A parsed HTTP/1.1 request.
 *
 * Only what the in-game panel actually sends is supported: a request line, headers, and an
 * optional body whose length is given by Content-Length. There is no chunked transfer encoding,
 * no multipart, no persistent-connection pipelining. This is deliberate - the server listens on
 * the loopback interface only and answers exactly one known client - and keeping it this small is
 * what makes it reviewable.
 *
 * \sa RemoteServer
 */
class REMOTE_API HttpRequest final
{
public:
    QString method;
    /*! The path with any query string removed, percent-decoding left untouched. */
    QString path;
    QHash<QString, QString> query;
    /*! Header names are lower-cased, since HTTP header names are case insensitive. */
    QHash<QString, QString> headers;
    QByteArray body;

    /*!
     * The outcome of feeding bytes to \c parse.
     */
    enum struct Result {
        /*! A complete request was parsed. */
        Complete,
        /*! The bytes received so far are a valid prefix of a request: read more. */
        Incomplete,
        /*! The bytes cannot be a valid request; the connection is to be closed. */
        Malformed
    };

    /*!
     * Attempts to parse one request out of \p data.
     *
     * \param data
     *        the bytes received so far
     * \param request
     *        filled in when the result is \c Complete; untouched otherwise
     * \param consumed
     *        set to the number of bytes the parsed request occupies, so that the caller can drop
     *        them from its buffer; only meaningful when the result is \c Complete
     * \return whether a complete request was parsed, more data is needed, or the input is invalid
     */
    static Result parse(const QByteArray &data, HttpRequest &request, int &consumed) noexcept;

    /*!
     * Returns the value of the header \p name, whose case is irrelevant.
     */
    std::optional<QString> getHeader(const QString &name) const noexcept;

    /*!
     * The largest request accepted, as a guard against a client that never stops sending. The
     * panel only ever posts a few dozen bytes of JSON.
     */
    static constexpr int MaxRequestSize {64 * 1024};
};

#endif // HTTPREQUEST_H

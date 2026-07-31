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
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "HttpRequest.h"

namespace
{
    constexpr const char *HeaderSeparator {"\r\n\r\n"};
    constexpr const char *LineSeparator {"\r\n"};

    void parseQueryString(const QString &queryString, QHash<QString, QString> &query) noexcept
    {
        const QStringList pairs = queryString.split('&', Qt::SkipEmptyParts);
        for (const QString &pair : pairs) {
            const auto separator = pair.indexOf('=');
            if (separator >= 0) {
                query.insert(QUrl::fromPercentEncoding(pair.left(separator).toUtf8()),
                             QUrl::fromPercentEncoding(pair.mid(separator + 1).toUtf8()));
            } else {
                query.insert(QUrl::fromPercentEncoding(pair.toUtf8()), QString());
            }
        }
    }
}

// PUBLIC

HttpRequest::Result HttpRequest::parse(const QByteArray &data, HttpRequest &request, int &consumed) noexcept
{
    if (data.size() > MaxRequestSize) {
        return Result::Malformed;
    }

    const auto headerEnd = data.indexOf(::HeaderSeparator);
    if (headerEnd < 0) {
        return Result::Incomplete;
    }

    const QString head = QString::fromUtf8(data.left(headerEnd));
    const QStringList lines = head.split(::LineSeparator);
    if (lines.isEmpty()) {
        return Result::Malformed;
    }

    // Request line: METHOD SP request-target SP HTTP-version
    const QStringList requestLine = lines.constFirst().split(' ', Qt::SkipEmptyParts);
    if (requestLine.size() < 2) {
        return Result::Malformed;
    }

    HttpRequest parsed;
    // An unknown verb still parses: a well formed request deserves a 405 from the caller rather
    // than a dropped connection, which is indistinguishable from the server being down
    parsed.method = requestLine.at(0).toUpper();

    const QString target = requestLine.at(1);
    const auto questionMark = target.indexOf('?');
    if (questionMark >= 0) {
        parsed.path = target.left(questionMark);
        ::parseQueryString(target.mid(questionMark + 1), parsed.query);
    } else {
        parsed.path = target;
    }
    if (parsed.path.isEmpty()) {
        return Result::Malformed;
    }

    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        const auto colon = line.indexOf(':');
        if (colon <= 0) {
            // A header line without a name is not something a real client sends
            return Result::Malformed;
        }
        parsed.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
    }

    const auto bodyStart = headerEnd + static_cast<int>(qstrlen(::HeaderSeparator));
    int contentLength {0};
    const auto contentLengthHeader = parsed.getHeader("content-length");
    if (contentLengthHeader) {
        bool ok {false};
        contentLength = contentLengthHeader->toInt(&ok);
        if (!ok || contentLength < 0) {
            return Result::Malformed;
        }
        if (contentLength > MaxRequestSize) {
            return Result::Malformed;
        }
    }

    if (data.size() - bodyStart < contentLength) {
        return Result::Incomplete;
    }
    parsed.body = data.mid(bodyStart, contentLength);

    request = parsed;
    consumed = bodyStart + contentLength;
    return Result::Complete;
}

std::optional<QString> HttpRequest::getHeader(const QString &name) const noexcept
{
    const auto it = headers.constFind(name.toLower());
    return it != headers.constEnd() ? std::optional<QString> {it.value()} : std::nullopt;
}

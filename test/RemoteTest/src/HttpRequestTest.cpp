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
#include <QtTest>
#include <QByteArray>
#include <QString>

#include <Remote/HttpRequest.h>

#include "HttpRequestTest.h"

namespace
{
    // Wraps the given lines into a request, so that the test data stays readable
    QByteArray request(const QByteArray &requestLine, const QByteArray &headers = {}, const QByteArray &body = {})
    {
        QByteArray data = requestLine + "\r\n";
        if (!headers.isEmpty()) {
            data += headers;
        }
        if (!body.isEmpty()) {
            data += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
        }
        data += "\r\n";
        data += body;
        return data;
    }
}

// PRIVATE SLOTS

void HttpRequestTest::initTestCase() noexcept
{}

void HttpRequestTest::cleanupTestCase() noexcept
{}

void HttpRequestTest::parseValidRequest_data() noexcept
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<QString>("expectedMethod");
    QTest::addColumn<QString>("expectedPath");
    QTest::addColumn<QByteArray>("expectedBody");

    QTest::newRow("GET without headers")
        << ::request("GET /api/state HTTP/1.1")
        << "GET" << "/api/state" << QByteArray {};

    QTest::newRow("GET with headers")
        << ::request("GET /api/state HTTP/1.1", "Host: 127.0.0.1:17285\r\nAccept: */*\r\n")
        << "GET" << "/api/state" << QByteArray {};

    QTest::newRow("POST with body")
        << ::request("POST /api/command HTTP/1.1", "Content-Type: application/json\r\n", R"({"command":"play"})")
        << "POST" << "/api/command" << QByteArray {R"({"command":"play"})"};

    QTest::newRow("lower case method is normalised")
        << ::request("get /api/state HTTP/1.1")
        << "GET" << "/api/state" << QByteArray {};

    QTest::newRow("unknown method still parses")
        << ::request("PATCH /api/state HTTP/1.1")
        << "PATCH" << "/api/state" << QByteArray {};

    QTest::newRow("query string is stripped from the path")
        << ::request("GET /api/flights?limit=5 HTTP/1.1")
        << "GET" << "/api/flights" << QByteArray {};

    QTest::newRow("empty body with explicit zero length")
        << ::request("POST /api/command HTTP/1.1", "Content-Length: 0\r\n")
        << "POST" << "/api/command" << QByteArray {};
}

void HttpRequestTest::parseValidRequest() noexcept
{
    // Setup
    QFETCH(QByteArray, data);
    QFETCH(QString, expectedMethod);
    QFETCH(QString, expectedPath);
    QFETCH(QByteArray, expectedBody);

    // Exercise
    HttpRequest request;
    int consumed {0};
    const HttpRequest::Result result = HttpRequest::parse(data, request, consumed);

    // Verify
    QCOMPARE(result, HttpRequest::Result::Complete);
    QCOMPARE(request.method, expectedMethod);
    QCOMPARE(request.path, expectedPath);
    QCOMPARE(request.body, expectedBody);
    QCOMPARE(consumed, data.size());
}

void HttpRequestTest::parseIncompleteRequest_data() noexcept
{
    QTest::addColumn<QByteArray>("data");

    QTest::newRow("empty") << QByteArray {};
    QTest::newRow("partial request line") << QByteArray {"GET /api/st"};
    QTest::newRow("no header terminator") << QByteArray {"GET /api/state HTTP/1.1\r\nHost: x\r\n"};
    QTest::newRow("body shorter than Content-Length")
        << QByteArray {"POST /api/command HTTP/1.1\r\nContent-Length: 20\r\n\r\n{\"command\""};
}

void HttpRequestTest::parseIncompleteRequest() noexcept
{
    // Setup
    QFETCH(QByteArray, data);

    // Exercise
    HttpRequest request;
    int consumed {0};
    const HttpRequest::Result result = HttpRequest::parse(data, request, consumed);

    // Verify
    QCOMPARE(result, HttpRequest::Result::Incomplete);
}

void HttpRequestTest::parseMalformedRequest_data() noexcept
{
    QTest::addColumn<QByteArray>("data");

    QTest::newRow("request line without a target")
        << QByteArray {"GET\r\n\r\n"};
    QTest::newRow("header without a name")
        << QByteArray {"GET /api/state HTTP/1.1\r\n: nonsense\r\n\r\n"};
    QTest::newRow("header without a colon")
        << QByteArray {"GET /api/state HTTP/1.1\r\nnonsense\r\n\r\n"};
    QTest::newRow("non numeric Content-Length")
        << QByteArray {"POST /api/command HTTP/1.1\r\nContent-Length: many\r\n\r\n"};
    QTest::newRow("negative Content-Length")
        << QByteArray {"POST /api/command HTTP/1.1\r\nContent-Length: -1\r\n\r\n"};
    QTest::newRow("Content-Length beyond the accepted size")
        << QByteArray {"POST /api/command HTTP/1.1\r\nContent-Length: 999999999\r\n\r\n"};
}

void HttpRequestTest::parseMalformedRequest() noexcept
{
    // Setup
    QFETCH(QByteArray, data);

    // Exercise
    HttpRequest request;
    int consumed {0};
    const HttpRequest::Result result = HttpRequest::parse(data, request, consumed);

    // Verify
    QCOMPARE(result, HttpRequest::Result::Malformed);
}

void HttpRequestTest::parseQueryString_data() noexcept
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("expectedValue");

    QTest::newRow("single parameter")
        << ::request("GET /api/flights?limit=5 HTTP/1.1") << "limit" << "5";
    QTest::newRow("second of two parameters")
        << ::request("GET /api/flights?limit=5&search=alps HTTP/1.1") << "search" << "alps";
    QTest::newRow("percent encoded value")
        << ::request("GET /api/flights?search=Mont%20Blanc HTTP/1.1") << "search" << "Mont Blanc";
    QTest::newRow("plus is not a space outside form encoding")
        << ::request("GET /api/flights?search=a+b HTTP/1.1") << "search" << "a+b";
    QTest::newRow("parameter without a value")
        << ::request("GET /api/flights?verbose HTTP/1.1") << "verbose" << "";
}

void HttpRequestTest::parseQueryString() noexcept
{
    // Setup
    QFETCH(QByteArray, data);
    QFETCH(QString, key);
    QFETCH(QString, expectedValue);

    // Exercise
    HttpRequest request;
    int consumed {0};
    const HttpRequest::Result result = HttpRequest::parse(data, request, consumed);

    // Verify
    QCOMPARE(result, HttpRequest::Result::Complete);
    QVERIFY(request.query.contains(key));
    QCOMPARE(request.query.value(key), expectedValue);
}

void HttpRequestTest::headerLookupIsCaseInsensitive() noexcept
{
    // Setup
    const QByteArray data = ::request("GET /api/state HTTP/1.1", "X-Sky-Dolly-Token: secret\r\n");

    // Exercise
    HttpRequest request;
    int consumed {0};
    const HttpRequest::Result result = HttpRequest::parse(data, request, consumed);

    // Verify
    QCOMPARE(result, HttpRequest::Result::Complete);
    QCOMPARE(request.getHeader("x-sky-dolly-token").value_or(QString()), QString("secret"));
    QCOMPARE(request.getHeader("X-SKY-DOLLY-TOKEN").value_or(QString()), QString("secret"));
    QVERIFY(!request.getHeader("x-absent").has_value());
}

void HttpRequestTest::parseConsumesExactlyOneRequest() noexcept
{
    // Setup: two requests arriving in one read, as a keep-alive connection may deliver them
    const QByteArray first = ::request("GET /api/state HTTP/1.1");
    const QByteArray second = ::request("POST /api/command HTTP/1.1", {}, R"({"command":"stop"})");
    QByteArray buffer = first + second;

    // Exercise
    HttpRequest request;
    int consumed {0};
    const HttpRequest::Result result = HttpRequest::parse(buffer, request, consumed);

    // Verify
    QCOMPARE(result, HttpRequest::Result::Complete);
    QCOMPARE(request.path, QString("/api/state"));
    QCOMPARE(consumed, first.size());

    // Exercise: the remainder must parse as the second request
    buffer.remove(0, consumed);
    HttpRequest next;
    int nextConsumed {0};
    const HttpRequest::Result nextResult = HttpRequest::parse(buffer, next, nextConsumed);

    // Verify
    QCOMPARE(nextResult, HttpRequest::Result::Complete);
    QCOMPARE(next.path, QString("/api/command"));
    QCOMPARE(next.body, QByteArray {R"({"command":"stop"})"});
    QCOMPARE(nextConsumed, second.size());
}

void HttpRequestTest::rejectOversizedRequest() noexcept
{
    // Setup: a client that never stops sending must not be able to grow the buffer without bound
    QByteArray data = "GET /api/state HTTP/1.1\r\n";
    data += QByteArray(HttpRequest::MaxRequestSize + 1, 'x');

    // Exercise
    HttpRequest request;
    int consumed {0};
    const HttpRequest::Result result = HttpRequest::parse(data, request, consumed);

    // Verify
    QCOMPARE(result, HttpRequest::Result::Malformed);
}

QTEST_MAIN(HttpRequestTest)
#include "HttpRequestTest.moc"

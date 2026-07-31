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
#ifndef HTTPREQUESTTEST_H
#define HTTPREQUESTTEST_H

#include <QObject>

/*!
 * Test cases for the HTTP request parser behind the in-game panel's API.
 *
 * The parser sits on a socket and is the first thing to see bytes from outside the application,
 * so the cases that matter are the malformed and the partial ones.
 */
class HttpRequestTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() noexcept;
    void cleanupTestCase() noexcept;

    void parseValidRequest_data() noexcept;
    void parseValidRequest() noexcept;

    void parseIncompleteRequest_data() noexcept;
    void parseIncompleteRequest() noexcept;

    void parseMalformedRequest_data() noexcept;
    void parseMalformedRequest() noexcept;

    void parseQueryString_data() noexcept;
    void parseQueryString() noexcept;

    void headerLookupIsCaseInsensitive() noexcept;
    void parseConsumesExactlyOneRequest() noexcept;
    void rejectOversizedRequest() noexcept;
};

#endif // HTTPREQUESTTEST_H

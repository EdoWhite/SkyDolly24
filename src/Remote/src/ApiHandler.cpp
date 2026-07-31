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
#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QString>
#include <QStringBuilder>
#include <QHash>
#include <QFile>
#include <QIODevice>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>

#include <Kernel/Const.h>
#include <Kernel/SimulatorVersion.h>
#include <Model/Flight.h>
#include <Model/FlightSummary.h>
#include <Model/Logbook.h>
#include <Model/Aircraft.h>
#include <Model/AircraftInfo.h>
#include <Model/AircraftType.h>
#include <Persistence/FlightSelector.h>
#include <Persistence/Service/FlightService.h>
#include <Persistence/Service/LogbookService.h>
#include <PluginManager/SkyConnectManager.h>
#include <PluginManager/Connect/SkyConnectIntf.h>
#include <PluginManager/Connect/Connect.h>

#include "ApiHandler.h"
#include "HttpRequest.h"

namespace
{
    constexpr const char *TokenHeader {"x-sky-dolly-token"};
    // How many logbook entries GET /api/flights returns by default. The panel shows a short list;
    // an unbounded query would stall the simulator's UI thread on a large logbook.
    constexpr int DefaultFlightLimit {25};
    constexpr int MaxFlightLimit {200};

    QString toString(Connect::State state) noexcept
    {
        switch (state) {
        case Connect::State::Disconnected: return QStringLiteral("Disconnected");
        case Connect::State::Connected: return QStringLiteral("Connected");
        case Connect::State::Recording: return QStringLiteral("Recording");
        case Connect::State::RecordingPaused: return QStringLiteral("RecordingPaused");
        case Connect::State::Replay: return QStringLiteral("Replay");
        case Connect::State::ReplayPaused: return QStringLiteral("ReplayPaused");
        }
        return QStringLiteral("Unknown");
    }

    QJsonObject toJson(const FlightSummary &summary) noexcept
    {
        QJsonObject object;
        object.insert("id", static_cast<qint64>(summary.flightId));
        object.insert("title", summary.title);
        object.insert("aircraftType", summary.aircraftType);
        object.insert("flightNumber", summary.flightNumber);
        object.insert("aircraftCount", static_cast<qint64>(summary.aircraftCount));
        object.insert("startLocation", summary.startLocation);
        object.insert("endLocation", summary.endLocation);
        object.insert("creationDate", summary.creationDate.isValid() ?
                                          summary.creationDate.toString(Qt::ISODate) : QString());
        return object;
    }
}

struct ApiHandlerPrivate
{
    QString accessToken;
    FlightService flightService;
    LogbookService logbookService;
};

// PUBLIC

ApiHandler::ApiHandler() noexcept
    : d {std::make_unique<ApiHandlerPrivate>()}
{}

ApiHandler::ApiHandler(ApiHandler &&rhs) noexcept = default;
ApiHandler &ApiHandler::operator=(ApiHandler &&rhs) noexcept = default;
ApiHandler::~ApiHandler() = default;

ApiHandler::Response ApiHandler::handle(const HttpRequest &request) noexcept
{
    // The panel is served from a coui:// origin, which browsers treat as opaque, so the preflight
    // has to be answered permissively. This is safe only because the socket never leaves the
    // loopback interface - see RemoteServer, which refuses any non-loopback peer.
    if (request.method == "OPTIONS") {
        return Response {204, "text/plain", {}, false};
    }

    if (!isAuthorised(request)) {
        return makeError(401, QStringLiteral("Invalid or missing access token"));
    }

    if (request.path == "/api/state") {
        return request.method == "GET" || request.method == "HEAD" ?
               handleState() : makeError(405, QStringLiteral("Use GET"));
    }
    if (request.path == "/api/events") {
        return request.method == "GET" ?
               Response {200, "text/event-stream", {}, true} : makeError(405, QStringLiteral("Use GET"));
    }
    if (request.path == "/api/command") {
        return request.method == "POST" ?
               handleCommand(request) : makeError(405, QStringLiteral("Use POST"));
    }
    if (request.path == "/api/flights") {
        return request.method == "GET" ?
               handleFlights(request) : makeError(405, QStringLiteral("Use GET"));
    }
    if (request.path == "/api/load") {
        return request.method == "POST" ?
               handleLoad(request) : makeError(405, QStringLiteral("Use POST"));
    }

    if (request.method == "GET" || request.method == "HEAD") {
        const Response panel = handlePanel(request.path);
        if (panel.statusCode == 200) {
            return panel;
        }
    }

    return makeError(404, QStringLiteral("No such endpoint: ") % request.path);
}

QJsonObject ApiHandler::getState() const noexcept
{
    auto &skyConnectManager = SkyConnectManager::getInstance();
    const Flight &flight = Logbook::getInstance().getCurrentFlight();
    const SimulatorVersion simulatorVersion = skyConnectManager.getSimulatorVersion();

    QJsonObject state;
    state.insert("connected", skyConnectManager.isConnected());
    state.insert("simulator", simulatorVersion.toString());
    state.insert("simulatorConnected", simulatorVersion.isValid());
    state.insert("state", ::toString(skyConnectManager.getState()));
    state.insert("recording", skyConnectManager.isInRecordingState());
    state.insert("replaying", skyConnectManager.isInReplayState());
    state.insert("paused", skyConnectManager.isPaused());
    state.insert("active", skyConnectManager.isActive());
    state.insert("timestamp", static_cast<qint64>(skyConnectManager.getCurrentTimestamp()));
    state.insert("duration", static_cast<qint64>(flight.getTotalDurationMSec()));
    state.insert("speed", static_cast<double>(skyConnectManager.getReplaySpeedFactor()));
    state.insert("atEnd", skyConnectManager.isAtEnd());

    QJsonObject flightObject;
    flightObject.insert("id", static_cast<qint64>(flight.getId()));
    flightObject.insert("title", flight.getTitle());
    flightObject.insert("flightNumber", flight.getFlightNumber());
    flightObject.insert("aircraftCount", static_cast<qint64>(flight.count()));
    const Aircraft &userAircraft = flight.getUserAircraft();
    flightObject.insert("aircraftType", userAircraft.getAircraftInfo().aircraftType.type);
    state.insert("flight", flightObject);

    return state;
}

bool ApiHandler::isAuthorised(const HttpRequest &request) const noexcept
{
    if (d->accessToken.isEmpty()) {
        return true;
    }
    const auto token = request.getHeader(::TokenHeader);
    return token && *token == d->accessToken;
}

void ApiHandler::setAccessToken(QString accessToken) noexcept
{
    d->accessToken = std::move(accessToken);
}

// PRIVATE

ApiHandler::Response ApiHandler::handleState() const noexcept
{
    return makeJson(getState());
}

ApiHandler::Response ApiHandler::handleCommand(const HttpRequest &request) noexcept
{
    QJsonParseError parseError {};
    const QJsonDocument document = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return makeError(400, QStringLiteral("Body is not a JSON object: ") % parseError.errorString());
    }
    const QJsonObject object = document.object();
    const QString command = object.value("command").toString();

    auto &skyConnectManager = SkyConnectManager::getInstance();
    if (command == "record") {
        skyConnectManager.startRecording(SkyConnectIntf::RecordingMode::SingleAircraft);
    } else if (command == "play") {
        // Starting from the beginning matches what the panel's play button means when the replay
        // has run to the end; mid-replay the current position is kept
        skyConnectManager.startReplay(skyConnectManager.isAtEnd());
    } else if (command == "pause") {
        skyConnectManager.setPaused(SkyConnectIntf::Initiator::App, !skyConnectManager.isPaused());
    } else if (command == "stop") {
        skyConnectManager.stop();
    } else if (command == "seek") {
        if (!object.contains("timestamp")) {
            return makeError(400, QStringLiteral("seek requires a timestamp"));
        }
        const auto timestamp = static_cast<std::int64_t>(object.value("timestamp").toDouble());
        skyConnectManager.seek(std::max<std::int64_t>(timestamp, 0), SkyConnectIntf::SeekMode::Discrete);
    } else if (command == "skipBegin") {
        skyConnectManager.skipToBegin();
    } else if (command == "skipEnd") {
        skyConnectManager.skipToEnd();
    } else if (command == "backward") {
        skyConnectManager.skipBackward();
    } else if (command == "forward") {
        skyConnectManager.skipForward();
    } else if (command == "speed") {
        const auto factor = object.value("factor").toDouble(1.0);
        if (factor <= 0.0) {
            return makeError(400, QStringLiteral("speed factor must be greater than zero"));
        }
        skyConnectManager.setReplaySpeedFactor(static_cast<float>(factor));
    } else if (command == "connect") {
        skyConnectManager.tryConnectAndSetup();
    } else {
        return makeError(400, QStringLiteral("Unknown command: ") % command);
    }

    // Answering with the resulting state saves the panel a follow-up request, and makes the
    // button it just pressed settle on the truth even if the command was a no-op
    return makeJson(getState());
}

ApiHandler::Response ApiHandler::handleFlights(const HttpRequest &request) const noexcept
{
    int limit {::DefaultFlightLimit};
    const auto limitParameter = request.query.constFind("limit");
    if (limitParameter != request.query.constEnd()) {
        bool ok {false};
        const int requested = limitParameter.value().toInt(&ok);
        if (ok && requested > 0) {
            limit = std::min(requested, ::MaxFlightLimit);
        }
    }

    FlightSelector selector;
    const auto searchParameter = request.query.constFind("search");
    if (searchParameter != request.query.constEnd()) {
        selector.searchKeyword = searchParameter.value();
    }

    bool ok {false};
    const std::vector<FlightSummary> summaries = d->logbookService.getFlightSummaries(selector, &ok);
    if (!ok) {
        return makeError(503, QStringLiteral("The logbook could not be read"));
    }

    // getFlightSummaries returns the whole selection; take the most recent entries, which is what
    // a "recent flights" list means
    QJsonArray flights;
    const auto count = static_cast<int>(summaries.size());
    for (int i = count - 1; i >= 0 && flights.size() < limit; --i) {
        flights.append(::toJson(summaries[static_cast<std::size_t>(i)]));
    }

    QJsonObject object;
    object.insert("flights", flights);
    object.insert("total", count);
    return makeJson(object);
}

ApiHandler::Response ApiHandler::handleLoad(const HttpRequest &request) noexcept
{
    QJsonParseError parseError {};
    const QJsonDocument document = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return makeError(400, QStringLiteral("Body is not a JSON object: ") % parseError.errorString());
    }
    const QJsonObject object = document.object();
    if (!object.contains("id")) {
        return makeError(400, QStringLiteral("load requires a flight id"));
    }
    const auto id = static_cast<std::int64_t>(object.value("id").toDouble());
    if (id == Const::InvalidId) {
        return makeError(400, QStringLiteral("Invalid flight id"));
    }

    auto &skyConnectManager = SkyConnectManager::getInstance();
    if (skyConnectManager.isActive()) {
        // Restoring the current flight underneath a running replay or recording would leave the
        // connect plugin sending data from a flight that no longer exists
        return makeError(409, QStringLiteral("Stop the current recording or replay first"));
    }

    Flight &flight = Logbook::getInstance().getCurrentFlight();
    if (!d->flightService.restoreFlight(id, flight)) {
        return makeError(404, QStringLiteral("No flight with that id in the logbook"));
    }
    return makeJson(getState());
}

ApiHandler::Response ApiHandler::handlePanel(const QString &path) const noexcept
{
    // A closed set of embedded resources, not a file server: there is no path that reaches the
    // file system, so there is nothing to traverse out of.
    static const QHash<QString, QByteArray> panelFiles {
        {QStringLiteral("/"), QByteArrayLiteral("text/html; charset=utf-8")},
        {QStringLiteral("/SkyDollyPanel.html"), QByteArrayLiteral("text/html; charset=utf-8")},
        {QStringLiteral("/SkyDollyPanel.css"), QByteArrayLiteral("text/css; charset=utf-8")},
        {QStringLiteral("/SkyDollyPanel.js"), QByteArrayLiteral("text/javascript; charset=utf-8")},
        {QStringLiteral("/favicon.svg"), QByteArrayLiteral("image/svg+xml")}
    };

    const auto it = panelFiles.constFind(path);
    if (it == panelFiles.constEnd()) {
        return Response {404, "application/json", {}, false};
    }

    const QString fileName = path == "/" ? QStringLiteral("SkyDollyPanel.html") : path.mid(1);
    QFile file {QStringLiteral(":/panel/") % fileName};
    if (!file.open(QIODevice::ReadOnly)) {
        return Response {404, "application/json", {}, false};
    }
    return Response {200, it.value(), file.readAll(), false};
}

ApiHandler::Response ApiHandler::makeJson(const QJsonObject &object, int statusCode) noexcept
{
    return Response {statusCode, "application/json", QJsonDocument(object).toJson(QJsonDocument::Compact), false};
}

ApiHandler::Response ApiHandler::makeError(int statusCode, const QString &message) noexcept
{
    QJsonObject object;
    object.insert("error", message);
    return makeJson(object, statusCode);
}

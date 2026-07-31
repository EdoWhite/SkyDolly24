/* Sky Dolly in-game panel.

   Talks to the Sky Dolly service over the loopback interface: a Server-Sent Events stream carries
   the state, plain fetch carries the commands.

   Written conservatively on purpose. The simulator runs this inside Coherent GT, a fork of an
   older Chromium, so there are no arrow functions, no template literals, no const/let, no optional
   chaining and no async/await here. It is the same file the Sky Dolly service serves over HTTP, so
   it also has to run unchanged in an ordinary desktop browser. */

(function () {
    'use strict';

    // The service listens on a fixed port. When this file is served by the service itself the page
    // origin is already correct; inside the simulator the origin is coui://, which is not a
    // reachable host, so fall back to the loopback address.
    var DEFAULT_PORT = 17285;
    var baseUrl = (function () {
        if (window.location && window.location.protocol &&
            window.location.protocol.indexOf('http') === 0) {
            return window.location.protocol + '//' + window.location.host;
        }
        return 'http://127.0.0.1:' + DEFAULT_PORT;
    })();

    var RECONNECT_DELAY_MSEC = 3000;

    var eventSource = null;
    var reconnectTimer = null;
    var lastState = null;
    var seeking = false;
    var flightsLoadedForId = null;

    function byId(id) {
        return document.getElementById(id);
    }

    function formatTime(msec) {
        if (!msec || msec < 0) {
            msec = 0;
        }
        var totalSeconds = Math.floor(msec / 1000);
        var hours = Math.floor(totalSeconds / 3600);
        var minutes = Math.floor((totalSeconds % 3600) / 60);
        var seconds = totalSeconds % 60;
        var text = (seconds < 10 ? '0' : '') + seconds;
        if (hours > 0) {
            return hours + ':' + (minutes < 10 ? '0' : '') + minutes + ':' + text;
        }
        return minutes + ':' + text;
    }

    /* Requests ------------------------------------------------------------ */

    function request(path, options) {
        var settings = options || {};
        settings.cache = 'no-store';
        return fetch(baseUrl + path, settings);
    }

    function sendCommand(command, extra) {
        var payload = { command: command };
        if (extra) {
            for (var key in extra) {
                if (Object.prototype.hasOwnProperty.call(extra, key)) {
                    payload[key] = extra[key];
                }
            }
        }
        return request('/api/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        }).then(function (response) {
            return response.json();
        }).then(function (state) {
            // The command answers with the resulting state, so the buttons settle on the truth
            // even when the command was a no-op
            if (state && !state.error) {
                applyState(state);
            }
            return state;
        })['catch'](function () {
            showOffline();
        });
    }

    /* Rendering ----------------------------------------------------------- */

    function showOffline() {
        byId('main').hidden = true;
        byId('offline').hidden = false;
        byId('offlineUrl').textContent = baseUrl;
        byId('statusDot').className = 'dot offline';
        byId('statusText').textContent = 'Service not reachable';
    }

    function showOnline() {
        byId('offline').hidden = true;
        byId('main').hidden = false;
    }

    function describe(state) {
        if (!state.connected) {
            return 'Sky Dolly running, simulator not connected';
        }
        if (state.recording) {
            return state.paused ? 'Recording paused' : 'Recording';
        }
        if (state.replaying) {
            return state.paused ? 'Replay paused' : 'Replaying';
        }
        return state.simulatorConnected ? state.simulator : 'Connected';
    }

    function applyState(state) {
        lastState = state;
        showOnline();

        var dotClass = 'dot online';
        if (state.recording) {
            dotClass = 'dot recording';
        } else if (state.replaying) {
            dotClass = 'dot replaying';
        } else if (!state.connected) {
            dotClass = 'dot offline';
        }
        byId('statusDot').className = dotClass;
        byId('statusText').textContent = describe(state);
        byId('statusText').title = state.simulator || '';

        var flight = state.flight || {};
        var hasFlight = flight.id && flight.id > 0;
        byId('flightTitle').textContent = hasFlight ?
            (flight.title || flight.flightNumber || ('Flight ' + flight.id)) :
            'No flight loaded';
        var sub = [];
        if (flight.aircraftType && flight.aircraftType !== '-') {
            sub.push(flight.aircraftType);
        }
        if (flight.aircraftCount > 1) {
            sub.push(flight.aircraftCount + ' aircraft');
        }
        byId('flightSub').textContent = sub.join(' · ');

        // Do not fight the user while they are dragging the timeline
        var seek = byId('seek');
        seek.max = state.duration || 0;
        seek.disabled = !state.duration;
        if (!seeking) {
            seek.value = state.timestamp || 0;
        }
        byId('elapsed').textContent = formatTime(state.timestamp);
        byId('duration').textContent = formatTime(state.duration);

        byId('play').innerHTML = (state.replaying && !state.paused) ? '&#10073;&#10073;' : '&#9654;';
        byId('record').className = state.recording ? 'record active' : 'record';

        var idle = !state.active;
        byId('record').disabled = !state.connected || state.replaying;
        byId('play').disabled = !state.connected || state.recording || !state.duration;
        byId('stop').disabled = idle;
        byId('skipBegin').disabled = !state.duration;
        byId('skipEnd').disabled = !state.duration;
        byId('backward').disabled = !state.duration;
        byId('forward').disabled = !state.duration;

        var speedSelect = byId('speedSelect');
        if (!speedSelect.matches(':focus')) {
            speedSelect.value = String(state.speed || 1);
        }

        // Reload the list when the loaded flight changed, so the highlight follows
        if (flightsLoadedForId !== flight.id) {
            flightsLoadedForId = flight.id;
            loadFlights();
        }
    }

    function renderFlights(data) {
        var list = byId('flightList');
        var hint = byId('flightsHint');
        list.innerHTML = '';

        var flights = (data && data.flights) || [];
        if (flights.length === 0) {
            hint.hidden = false;
            hint.textContent = 'The logbook is empty. Record a flight to see it here.';
            return;
        }
        hint.hidden = true;

        for (var i = 0; i < flights.length; ++i) {
            (function (flight) {
                var item = document.createElement('li');
                if (lastState && lastState.flight && lastState.flight.id === flight.id) {
                    item.className = 'current';
                }

                var row = document.createElement('div');
                row.className = 'flightRow';
                var title = document.createElement('span');
                title.textContent = flight.title || flight.flightNumber || ('Flight ' + flight.id);
                var date = document.createElement('span');
                date.textContent = (flight.creationDate || '').replace('T', ' ').substring(0, 16);
                row.appendChild(title);
                row.appendChild(date);

                var sub = document.createElement('div');
                sub.className = 'flightRowSub';
                var parts = [];
                if (flight.aircraftType) { parts.push(flight.aircraftType); }
                if (flight.startLocation || flight.endLocation) {
                    parts.push((flight.startLocation || '?') + ' → ' + (flight.endLocation || '?'));
                }
                sub.textContent = parts.join(' · ');

                item.appendChild(row);
                item.appendChild(sub);
                item.onclick = function () {
                    loadFlight(flight.id);
                };
                list.appendChild(item);
            })(flights[i]);
        }
    }

    function loadFlights() {
        request('/api/flights?limit=25').then(function (response) {
            return response.json();
        }).then(renderFlights)['catch'](function () {
            var hint = byId('flightsHint');
            hint.hidden = false;
            hint.textContent = 'The logbook could not be read.';
        });
    }

    function loadFlight(id) {
        request('/api/load', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ id: id })
        }).then(function (response) {
            return response.json().then(function (body) {
                return { ok: response.ok, body: body };
            });
        }).then(function (result) {
            if (result.ok) {
                applyState(result.body);
                // The highlight has to move even though the flight id may be unchanged
                flightsLoadedForId = null;
            } else {
                var hint = byId('flightsHint');
                hint.hidden = false;
                hint.textContent = result.body.error || 'The flight could not be loaded.';
            }
        })['catch'](function () {
            showOffline();
        });
    }

    /* Event stream -------------------------------------------------------- */

    function connect() {
        if (reconnectTimer) {
            clearTimeout(reconnectTimer);
            reconnectTimer = null;
        }
        if (eventSource) {
            eventSource.close();
            eventSource = null;
        }

        try {
            eventSource = new EventSource(baseUrl + '/api/events');
        } catch (error) {
            scheduleReconnect();
            return;
        }

        eventSource.onmessage = function (event) {
            try {
                applyState(JSON.parse(event.data));
            } catch (error) {
                // A malformed frame is not worth tearing the stream down for
            }
        };

        eventSource.onerror = function () {
            // EventSource retries on its own, but not when the service was never there in the
            // first place, which is the common case: the panel is often opened first
            if (eventSource) {
                eventSource.close();
                eventSource = null;
            }
            showOffline();
            scheduleReconnect();
        };
    }

    function scheduleReconnect() {
        if (reconnectTimer) {
            return;
        }
        reconnectTimer = setTimeout(function () {
            reconnectTimer = null;
            connect();
        }, RECONNECT_DELAY_MSEC);
    }

    /* Wiring -------------------------------------------------------------- */

    function wire() {
        byId('record').onclick = function () { sendCommand('record'); };
        byId('stop').onclick = function () { sendCommand('stop'); };
        byId('skipBegin').onclick = function () { sendCommand('skipBegin'); };
        byId('skipEnd').onclick = function () { sendCommand('skipEnd'); };
        byId('backward').onclick = function () { sendCommand('backward'); };
        byId('forward').onclick = function () { sendCommand('forward'); };
        byId('retry').onclick = function () { connect(); };
        byId('refresh').onclick = function () { loadFlights(); };

        byId('play').onclick = function () {
            // One button for both, following what the state says rather than what was pressed
            // last. While replaying, the pause command toggles in both directions.
            if (lastState && lastState.replaying) {
                sendCommand('pause');
            } else {
                sendCommand('play');
            }
        };

        byId('speedSelect').onchange = function () {
            sendCommand('speed', { factor: parseFloat(this.value) });
        };

        var seek = byId('seek');
        seek.oninput = function () {
            seeking = true;
            byId('elapsed').textContent = formatTime(parseInt(this.value, 10));
        };
        seek.onchange = function () {
            seeking = false;
            sendCommand('seek', { timestamp: parseInt(this.value, 10) });
        };
    }

    function start() {
        wire();
        showOffline();
        connect();
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', start);
    } else {
        start();
    }
})();

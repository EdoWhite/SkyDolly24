/* Sky Dolly in-game panel.

   Talks to the Sky Dolly service over the loopback interface, polling for state and posting
   commands, over XMLHttpRequest throughout.

   Written conservatively on purpose, and more conservatively than it first was. The simulator runs
   this inside Coherent GT, a fork of an older Chromium: no arrow functions, no template literals,
   no const/let, no optional chaining, no async/await - and, as the first in-simulator test showed,
   nothing may be assumed about fetch, Promise or EventSource either. A missing fetch does not fail
   politely: it throws straight out of the click handler, so the button does nothing and says
   nothing, which is exactly how this presented itself. XMLHttpRequest and callbacks are available
   in every engine this has to run in, including an ordinary desktop browser, which serves the same
   file from the Sky Dolly service itself. */

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

    // How often the state is asked for. The service answers from memory over loopback, so this is
    // cheap; it replaces a Server-Sent Events stream, which needed EventSource
    var POLL_INTERVAL_MSEC = 250;

    var pollTimer = null;
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

    /*
     * Sends one request and calls back. Nothing here throws: every failure - a missing
     * XMLHttpRequest, a refused connection, a body that is not JSON - arrives at onFailure with
     * something a human can read, because a panel that fails silently is a panel nobody can
     * report a fault in.
     */
    function request(method, path, body, onSuccess, onFailure) {
        function fail(message) {
            if (onFailure) {
                onFailure(message);
            }
        }

        var xhr = null;
        try {
            xhr = new XMLHttpRequest();
            xhr.open(method, baseUrl + path, true);
            if (body !== null && body !== undefined) {
                xhr.setRequestHeader('Content-Type', 'application/json');
            }
        } catch (error) {
            fail('Cannot reach ' + baseUrl + ': ' + error);
            return;
        }

        xhr.onreadystatechange = function () {
            if (xhr.readyState !== 4) {
                return;
            }
            var parsed = null;
            try {
                parsed = xhr.responseText ? JSON.parse(xhr.responseText) : null;
            } catch (error) {
                parsed = null;
            }
            // Status 0 is what a refused or blocked connection looks like from here
            if (xhr.status >= 200 && xhr.status < 300) {
                if (onSuccess) {
                    onSuccess(parsed);
                }
            } else if (xhr.status === 0) {
                fail('No answer from ' + baseUrl + ' - is Sky Dolly running?');
            } else {
                fail((parsed && parsed.error) || ('HTTP ' + xhr.status));
            }
        };

        try {
            xhr.send(body === null || body === undefined ? null : JSON.stringify(body));
        } catch (error) {
            fail('Request failed: ' + error);
        }
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
        request('POST', '/api/command', payload, function (state) {
            // The command answers with the resulting state, so the buttons settle on the truth
            // even when the command was a no-op
            clearProblem();
            if (state && !state.error) {
                applyState(state);
            } else if (state && state.error) {
                reportProblem(state.error);
            }
        }, function (message) {
            reportProblem('"' + command + '" failed: ' + message);
            showOffline();
        });
    }

    /* Rendering ----------------------------------------------------------- */

    // The hidden attribute is set through the class as well as the property: it relies on a rule in
    // the engine's default stylesheet, and inside the simulator that cannot be taken on trust. The
    // stylesheet here carries the rule, and this keeps the two in step
    function setHidden(id, hide) {
        var element = byId(id);
        if (!element) {
            return;
        }
        element.hidden = hide;
        if (hide) {
            element.setAttribute('hidden', 'hidden');
        } else {
            element.removeAttribute('hidden');
        }
    }

    function reportProblem(message) {
        var problem = byId('problem');
        if (problem) {
            problem.textContent = message;
            setHidden('problem', false);
        }
    }

    function clearProblem() {
        setHidden('problem', true);
    }

    function showOffline() {
        setHidden('main', true);
        setHidden('offline', false);
        byId('offlineUrl').textContent = baseUrl;
        byId('statusDot').className = 'dot offline';
        byId('statusText').textContent = 'Service not reachable';
    }

    function showOnline() {
        setHidden('offline', true);
        setHidden('main', false);
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

        // activeElement rather than matches(':focus'): matches is another thing not worth assuming
        var speedSelect = byId('speedSelect');
        if (document.activeElement !== speedSelect) {
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
            setHidden('flightsHint', false);
            hint.textContent = 'The logbook is empty. Record a flight to see it here.';
            return;
        }
        setHidden('flightsHint', true);

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
        request('GET', '/api/flights?limit=25', null, renderFlights, function (message) {
            var hint = byId('flightsHint');
            setHidden('flightsHint', false);
            hint.textContent = 'The logbook could not be read: ' + message;
        });
    }

    function loadFlight(id) {
        request('POST', '/api/load', { id: id }, function (state) {
            clearProblem();
            applyState(state);
            // The highlight has to move even though the flight id may be unchanged
            flightsLoadedForId = null;
        }, function (message) {
            var hint = byId('flightsHint');
            setHidden('flightsHint', false);
            hint.textContent = message;
        });
    }

    /* State ---------------------------------------------------------------- */

    // Polled rather than streamed. The service does offer a Server-Sent Events stream, and it is
    // the better transport where EventSource exists - but it cannot be relied on inside the
    // simulator, and one transport that works everywhere beats two that need testing separately.
    function poll() {
        request('GET', '/api/state', null, function (state) {
            if (state) {
                clearProblem();
                applyState(state);
            }
        }, function (message) {
            showOffline();
            reportProblem(message);
        });
    }

    function connect() {
        if (pollTimer) {
            clearInterval(pollTimer);
            pollTimer = null;
        }
        poll();
        pollTimer = setInterval(poll, POLL_INTERVAL_MSEC);
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
        // Anything that throws in here used to leave a panel that looked finished and did nothing.
        // Now it says so on the panel itself, which is the only place the person who hit it can see
        try {
            wire();
            showOffline();
            clearProblem();
            connect();
        } catch (error) {
            reportProblem('Sky Dolly panel failed to start: ' + error);
        }
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', start);
    } else {
        start();
    }
})();

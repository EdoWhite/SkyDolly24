# Sky Dolly → MSFS 2024: stabilità, allineamento replay e controllo in-game

## Context

Questa repo è un fork di [till213/SkyDolly](https://github.com/till213/SkyDolly) v0.20.0: un'app Qt6/C++20 per Windows che registra e riproduce voli in **Microsoft Flight Simulator 2020** via SimConnect. Viene usata con **MSFS 2024**, per cui non è mai stata progettata: nel codice non esiste alcuna nozione di MSFS 2024 (`src/Kernel/include/Kernel/FlightSimulator.h:36` ha solo `MSFS` e `Prepar3Dv5`), viene compilata contro l'SDK 2020, e `isRunning()` cerca il processo `FlightSimulator.exe` invece di `FlightSimulator2024.exe`.

I tre problemi riportati hanno cause identificate:

1. **Crash frequenti.** In `dispatch()` (`MSFSSimConnectPlugin.cpp:1304`) la ricezione di `SIMCONNECT_RECV_ID_QUIT` chiude l'handle SimConnect e ne apre subito uno nuovo **dall'interno della callback `SimConnect_CallDispatch` di quello stesso handle**. MSFS 2024 emette QUIT/Pause in modo diverso dal 2020 (lo manda anche uscendo al menu principale), quindi questo percorso re-entrante viene colpito spesso. A ciò si aggiungono: `d->simConnectAi` dereferenziato senza null-check a `:1289` mentre `closeConnection()` lo azzera, `SimConnect_CallDispatch(nullptr, …)` possibile a `:1356`, e `SIMCONNECT_RECV_ID_EXCEPTION` gestito **solo in build Debug** (`:1320-1330`) — in release ogni errore del server è silenzioso. Su Windows non c'è `SetUnhandledExceptionFilter`, nessun minidump e nessun file di log: un access violation termina il processo senza lasciare traccia.

2. **Replay non allineato / a scatti.** La posizione viene registrata a **1 Hz** (`updateRequestPeriod()`, `MSFSSimConnectPlugin.cpp:734`, `SIMCONNECT_PERIOD_SECOND`, con il commento «sample the position data only at 1 Hz, in order to smoothen the curve») mentre l'assetto è a `SIMCONNECT_PERIOD_SIM_FRAME` (~60 Hz). In virata la spline di Hermite su 1 secondo di buco devia dalla traiettoria reale e va fuori fase rispetto all'assetto → è esattamente lo stutter segnalato in [#184](https://github.com/till213/SkyDolly/issues/184). Inoltre pitch/bank/heading sono interpolati come tre scalari Euleriani indipendenti (`src/Model/src/Attitude.cpp:49-92`), cosa che produce oscillazione in virata, e l'orologio di replay è un `QElapsedTimer` wall-clock (`AbstractSkyConnect.cpp:745-768`) non agganciato ai frame del sim. L'integratore di correzione quota ASRA (`currentAltitudeOffset`) non viene **mai** azzerato.

3. **Motori e suoni sfasati.** Sky Dolly registra solo leve e stati binari: throttle, elica, miscela, batteria, starter, `GENERAL ENG COMBUSTION`. **Non registra alcun RPM, N1/N2, coppia, pressione di alimentazione o flusso carburante** (sono nella wishlist `doc/Potential Variables.simvars`, mai implementati). Il suono motore in MSFS deriva dagli RPM/N1 realmente simulati: in replay questi vengono ricalcolati dall'aereo a partire dal throttle, con la sua dinamica di spool-up → sfasamento. Per l'accensione si usa la macchina a stati `ENGINE_AUTO_START`/`ENGINE_AUTO_SHUTDOWN` (`Event/EventStateHandler.h:367-441`) che sugli addon con sistemi propri (Fenix, PMDG, iniBuilds, Milviz) non funziona → [#178](https://github.com/till213/SkyDolly/issues/178) motori che si spengono da soli, [#185](https://github.com/till213/SkyDolly/issues/185) sequenza di avviamento Fenix non registrata. Le eliche del Joby S4 non girano ([#197](https://github.com/till213/SkyDolly/issues/197)) perché non esiste `PROP RPM` né gestione dei motori elettrici del 2024.

4. **Nessuna integrazione in-game.** Non esiste **alcuna** superficie di rete o IPC nel progetto (Qt è linkato con `Widgets Sql LinguistTools`, niente `Network`). L'unico canale dal sim sono le scorciatoie da tastiera SimConnect già esistenti (`Event/InputEvent.cpp`), che però sono solo in ingresso e senza feedback visivo.

**Esito atteso:** non un'applicazione desktop da lanciare, ma un **add-on per MSFS 2024 in stile GSX**: un pacchetto nella cartella `Community` che aggiunge un'icona alla toolbar del simulatore, più un processo di servizio in background che il sim avvia da solo all'avvio. Deve non crashare e riprodurre voli allineati e fluidi, con motori e suoni coerenti su aerei nativi 2024, GA a elica e addon complessi.

> **Architettura scelta (su tua indicazione: "può essere qualunque cosa che poi posso usare dal simulatore, un plugin come GSX").** GSX funziona esattamente così: `Couatl64_MSFS.exe` è un processo in background, e tutta la UI è un pannello nella toolbar del sim. Un processo separato resta tecnicamente necessario — un modulo WASM in-sim non può usare Qt, SQLite o il filesystem come serve qui, e riscrivere tutto in WASM significherebbe buttare il motore di registrazione, il logbook e tutti i plugin di import/export — ma diventa **invisibile**: lo lancia MSFS, non tu. La finestra desktop resta disponibile come strumento avanzato (logbook, import/export, query SQL) e non serve per volare.

**Decisioni prese con l'utente:** consegna come add-on del simulatore (pannello in toolbar come UI primaria) con overlay di ripiego; build verificata in **CI GitHub Actions** (io lavoro su macOS e non posso compilare né provare contro MSFS); priorità su aerei nativi 2024, addon complessi e GA a elica; **modulo Formation deprioritizzato** (volo singolo).

**Assunzione dichiarata:** target unico MSFS 2024. Non forkiamo il plugin né rimuoviamo i percorsi 2020 (costerebbe più del beneficio), ma il rilevamento a runtime della versione decide il comportamento e solo 2024 viene testato e documentato.

---

## Fase 0 — Build riproducibile e CI (abilita tutto il resto)

Senza questo non esiste modo di verificare nulla.

- **Vendorizzare SimConnect 2024** in `3rdParty/SimConnect/` (`include/SimConnect.h`, `lib/SimConnect.lib`, `lib/SimConnect.dll`). Licenza MIT (`THIRD_PARTY.md:12`) e la DLL è già ridistribuita nelle release, quindi è consentito. È ciò che permette alla CI di compilare senza l'SDK installato.
- Riscrivere `cmake/FindSimConnect.cmake`: ordine di ricerca `MSFS2024_SDK` → `C:/MSFS 2024 SDK/` → `MSFS_SDK` → copia vendorizzata. Sistemare i bug attuali: `SimConnect_FOUND` non viene mai impostato (quindi `find_package(... REQUIRED)` in `src/Plugins/Connect/CMakeLists.txt:7` non può fallire), `if(${MSFS_SDK_INSTALLED})` si espande a `if()` vuoto quando la variabile non è definita, e `MSFS_SDK_FOUND` a `:11` non esiste.
- Sistemare `cmake/InitSubmodules.cmake:20`: il `foreach` itera `SUBMODULE_TEST_FILES` invece di `GIT_SUBMODULE_TEST_FILES`, quindi il controllo è codice morto; e `WORKING_DIRECTORY` punta a `cmake/` invece che alla root.
- **Nuovo workflow `.github/workflows/windows-release.yml`**: Windows runner, **MSVC** (non MinGW — evita il bug `windres`/spazi documentato in `BUILD.md:101` e non costringe a spedire `libgcc_s_seh-1.dll`/`libstdc++-6.dll`), Qt 6.8, `submodules: recursive`, build Release, poi `windeployqt` e assemblaggio del pacchetto secondo `RELEASE.md:63-81` (esclusi `*Test.exe`, `Plugins/Connect/PathCreator.dll`, `Plugins/Module/Template.dll`), zip caricato come artifact. Attualmente **non esiste alcuna automazione di packaging**: niente `windeployqt`, niente `install()`, niente CPack — solo una checklist manuale.
- Nello stesso workflow, zippare anche il pacchetto Community della Fase 5.

*File chiave:* `cmake/FindSimConnect.cmake`, `cmake/InitSubmodules.cmake`, `src/Plugins/Connect/CMakeLists.txt`, `.github/workflows/`, `RELEASE.md`, `BUILD.md`.

---

## Fase 1 — Stabilità: eliminare i crash

- **Handler di crash Windows** in `src/SkyDolly/src/`: `SetUnhandledExceptionFilter` + `_set_purecall_handler` + `_set_invalid_parameter_handler` che scrivono un minidump e il report testuale in `%LOCALAPPDATA%/SkyDolly/crash/`. Riusare `Kernel::StackTrace::generate()` (`src/Kernel/src/StackTrace.cpp:86`) e la `TerminationDialog::createReport()` già esistente (`src/UserInterface/src/Dialog/TerminationDialog.cpp:107`), che oggi finisce solo negli appunti. `SignalHandler_Windows.cpp:33-40` è oggi uno stub vuoto: è lì che va agganciato.
- **File di log**: `qInstallMessageHandler` con rotazione in `%LOCALAPPDATA%/SkyDolly/logs/`. Oggi l'app è `WIN32_EXECUTABLE`, quindi tutti i `qDebug`/`qCritical` sono invisibili.
- **Nessuna chiamata SimConnect re-entrante dalla callback `dispatch`.** Questa è la correzione singola più importante. Ogni azione che chiude/riapre l'handle o cambia stato deve essere posticipata con `QMetaObject::invokeMethod(this, …, Qt::QueuedConnection)`:
  - `SIMCONNECT_RECV_ID_QUIT` (`MSFSSimConnectPlugin.cpp:1304`) → segnala e gestisci fuori dalla callback;
  - evento `Crashed` (`:880`) → `stopRecording()`/`stopReplay()` posticipati;
  - `emit actionActivated(...)` (`:900, 907, 914, 921, 966, 973`) → connessione in coda, perché oggi eseguono slot di UI che possono chiamare `SimConnect_Open` annidato.
- Null-guard su `d->simConnectAi` in `SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID` (`:1289`) e su `d->simConnectHandle` in `processSimConnectEvent()` (`:1356`).
- Gestire `SIMCONNECT_RECV_ID_EXCEPTION` **anche in release** (`:1320`): decodificare il codice, loggarlo, e portare in UI gli errori ripetuti (oggi un `SetDataOnSimObject` rifiutato da MSFS 2024 è invisibile).
- `SkyConnectManager::getCurrentSkyConnect()` (`src/PluginManager/src/SkyConnectManager.cpp:134-142`) dereferenzia un `qobject_cast` potenzialmente nullo — usato da ~40 metodi. Aggiungere il controllo.
- Sostituire il pattern `HRESULT result |= …; if (result == S_OK)` (diffuso in `Event/EventStateHandler.h`, `Event/SimulationTime.h`, `Event/InputEvent.cpp`) con `FAILED()`/`SUCCEEDED()`.
- Bug secchi da correggere subito: `Event/EventStateHandler.h:236` assegna `m_strobeLightToggle.requested = event.taxi` (dovrebbe essere `event.strobe`); `Event/InputEvent.cpp:58-59` ha `Stop = 0x8` e `Forward = 0x8` in collisione; `ConnectPluginBaseSettings.cpp:61` inizializza sette membri su otto, per cui `record` eredita la scorciatoia di replay e `end` resta vuota.

---

## Fase 2 — Puntare davvero a MSFS 2024

- **Compilare contro l'SDK 2024.** La documentazione SDK è esplicita: un modulo compilato con l'SDK 2020 viene rilevato come modulo 2020 e funziona in modalità legacy, senza le nuove funzionalità. Attenzione al breaking change noto: `ident` passa a 8 caratteri in `SIMCONNECT_ICAO` / `SIMCONNECT_DATA_FACILITY_AIRPORT` (non usate qui, ma da verificare in compilazione).
- **Rilevamento della versione a runtime**: leggere il payload di `SIMCONNECT_RECV_ID_OPEN` (`:1314`, oggi solo un `qDebug`) — `szApplicationName`, `dwApplicationVersionMajor/Minor`, `dwSimConnectVersion*` — e memorizzarlo in un nuovo tipo `SimulatorVersion`. Mostrarlo nella dialog About e usarlo per le poche diramazioni di comportamento.
- `src/Kernel/include/Kernel/FlightSimulator.h`: aggiungere `MSFS2024`. In `FlightSimulator_Windows.cpp` correggere `isRunning()` (cerca `FlightSimulator2024.exe`) e `isInstalled()`, che oggi **ignora del tutto il proprio argomento** e controlla solo percorsi 2020 con un path malformato (`%APPDATA%` punta già a `Roaming`, quindi `…/Roaming/Local/Packages/…` non esiste). Percorsi 2024 corretti: Steam `%APPDATA%\Microsoft Flight Simulator 2024`, MS Store `%LOCALAPPDATA%\Packages\Microsoft.Limitless_8wekyb3d8bbwe\LocalCache`.
- Aggiornare `MSFSSimConnectPlugin.json` (`flightSimulator`, `name`) e `FlightSimulator::nameToId()`; aggiornare i commenti di `res/SimConnect.cfg`, che documentano i percorsi 2020.
- **Semantica pause/stato cambiata nel 2024.** In MSFS 2024 `SimStop` non viene inviato uscendo al menu, il flag di pausa è attivo nel menu e si disattiva appena si preme Fly, e `Pause_EX1` ha comportamenti anomali. La logica attuale (`:866-878`, con lo swallow della prima "unpause") è tarata sul 2020 ed è la causa più probabile del ciclo play‑2s / pausa‑breve segnalato in [#186](https://github.com/till213/SkyDolly/issues/186). Sottoscrivere `Pause_EX1` + `SimStart`/`SimStop` e ricostruire una macchina a stati "sono davvero in volo" esplicita.
- **Connessione robusta**: `SimConnect_Open` può restituire `E_FAIL` in modo persistente su alcune build 2024. Mantenere il backoff di Fibonacci (`AbstractSkyConnect.cpp:83`) ma esporre lo stato reale in UI invece di ritentare in silenzio.
- **Teletrasporto**: `SIMCONNECT_DATA_INITPOSITION` su lunghe distanze **congela MSFS 2024** (bug noto e riproducibile con il sample SetData dell'SDK; non si manifesta entro poche miglia). È usato da `onInitialPositionSetup()` (`:176-185`) all'inizio di ogni replay e dal teletrasporto delle Location. Sostituire con un riposizionamento incrementale o con `SetDataOnSimObject` di lat/lon/alt ad aereo già congelato, tenendo INITPOSITION solo per spostamenti brevi. Questo è probabilmente ciò che l'utente percepisce come "crash".
- **Modalità "tempo di replay"**: gli eventi `ZULU_YEAR/DAY/HOURS/MINUTES_SET` usati da `Event/SimulationTime.h:43-79` **non funzionano più in MSFS 2024** (corrisponde al punto 1 di [#185](https://github.com/till213/SkyDolly/issues/185)). Verificare in implementazione se esiste un'API 2024 equivalente; in caso negativo disattivare la funzione con un messaggio esplicito invece di lasciarla fallire in silenzio.

---

## Fase 3 — Allineamento e fluidità del replay

1. **Registrare la posizione alla frequenza dei frame**, non a 1 Hz. In `updateRequestPeriod()` (`MSFSSimConnectPlugin.cpp:731-784`) portare `PositionAll` da `SIMCONNECT_PERIOD_SECOND` a `SIMCONNECT_PERIOD_SIM_FRAME`, con frequenza configurabile. Il tipo `SampleRate::SampleRate` (`src/Kernel/include/Kernel/SampleRate.h`) è già incluso dal plugin ma **mai usato**: va collegato all'option widget del plugin.
   - *Costo:* le righe della tabella `position` passano da ~1/s a ~30–60/s. Mitigazione: decimazione adattiva in scrittura (si salva un campione solo se la predizione di Hermite dai precedenti devia oltre una soglia) e inserimenti in batch — oggi `SQLiteAircraftDao::insertAircraftData()` fa una `INSERT` per campione.
2. **Interpolare l'assetto con SLERP su quaternioni** invece di tre Hermite Euleriani indipendenti. Aggiungere `SkyMath::slerp` accanto a `interpolateHermite*` (`src/Kernel/include/Kernel/SkyMath.h:180-265`) e usarlo in `Attitude::interpolate` (`src/Model/src/Attitude.cpp:49-92`), mantenendo lo storage in Eulero. È la correzione mirata all'oscillazione in virata.
3. **Agganciare l'orologio di replay ai frame del sim.** `updateCurrentTimestamp()` (`AbstractSkyConnect.cpp:745-768`) è chiamato una volta per messaggio Windows, non per frame renderizzato. Far avanzare `currentTimestamp` con il delta misurato **fra due `SIMCONNECT_RECV_ID_EVENT_FRAME`** (che è già l'evento che guida il replay, `:1270`), così a ogni frame corrisponde esattamente una scrittura di posizione, eliminando il battimento fra timer host e frame rate del sim.
4. **Azzerare l'integratore ASRA.** `currentAltitudeOffset` (`MSFSSimConnectPlugin.cpp:121`, aggiornato a `:1254`) accumula per tutta la sessione, attraverso seek, riavvii di replay e riconnessioni, senza alcun reset. Azzerarlo su seek/start/stop/riconnessione e limitarlo. Candidato diretto per il carrello che sprofonda in pista ([#155](https://github.com/till213/SkyDolly/issues/155)).
5. **Seek**: `resetEventStates(Seek)` azzera la macchina a stati del motore, per cui ogni seek può far ripartire un `ENGINE_AUTO_START`. Ricostruire lo stato senza rieseguire l'avviamento quando la combustione non è cambiata.
6. Verificare che velocità e velocità angolari registrate (`VELOCITY BODY X/Y/Z`) vengano effettivamente inviate insieme a posizione e assetto, così che animazioni e blending del sim restino coerenti.

---

## Fase 4 — Fedeltà di motori e suoni

- **Registrare la strumentazione motore mancante**, seguendo la convenzione di `src/Plugins/Connect/MSFSSimConnectPlugin/doc/SimVars.md` (nuovo sotto-record `SimConnectEngineAnimation` da comporre in `…EngineAll`/`…EngineAi`): `GENERAL ENG RPM:1..4`, `PROP RPM:1..4`, `TURB ENG N1/N2:1..4`, `ENG TORQUE`, `RECIP ENG MANIFOLD PRESSURE`, `RECIP ENG FUEL FLOW`, `ENG ROTOR RPM` (elicotteri). Sono esattamente le variabili elencate in `doc/Potential Variables.simvars` e mai implementate.
  - Comporta nuovi campi in `EngineData` (`src/Model/`) e **una nuova migrazione DB**: si aggiungono tag `@migr(id = "<UUID nuovo>", …)` in coda a `src/Persistence/src/Dao/SQLite/migr/LogbookMigration.sql` (le migrazioni non sono file separati, sono marcatori con chiave `(id, step)`), più le colonne in `SQLiteEngineDao`.
- **Nuova impostazione "modalità replay motori"**, in tre modi, per risolvere il conflitto con gli aerei che simulano i propri sistemi:
  - *Simvar* — scrittura diretta di RPM/N1/prop RPM e combustione a ogni frame: massima sincronia audio sugli aerei nativi 2024 e GA.
  - *Solo leve/eventi* — solo throttle, miscela, elica, starter e combustione via evento: percorso sicuro per Fenix/PMDG/iniBuilds/Milviz, i cui moduli WASM riscrivono le simvar a ogni frame e vincono sempre sulla scrittura esterna.
  - *Auto* (default) — tenta *Simvar* e ripiega su *Solo leve* quando il sim rifiuta la scrittura (rilevabile ora che le `SIMCONNECT_RECV_ID_EXCEPTION` sono gestite, Fase 1).
  Da persistere per tipo di aereo: il tipo è già registrato in `AircraftInfo::aircraftType`.
- **Sostituire la macchina a stati di avviamento** (`Event/EventStateHandler.h:367-441`), oggi basata su `ENGINE_AUTO_START`/`ENGINE_AUTO_SHUTDOWN` globali, con eventi per singolo motore (`TOGGLE_STARTER{N}`, `SET_STARTER{N}_HELD`, `MIXTURE{N}_SET`, valvole carburante) più scrittura diretta di `ENG COMBUSTION:{N}` quando la modalità lo consente, mantenendo AUTO_START come ripiego. Mira a [#178](https://github.com/till213/SkyDolly/issues/178) e alla sequenza Fenix di [#185](https://github.com/till213/SkyDolly/issues/185).
- **Motori elettrici / eVTOL (novità 2024)**: diramare su `ENGINE TYPE` (già registrato in `SimConnectAircraftInfo`) e gestire le simvar dei motori elettrici, così che le eliche del Joby S4 girino ([#197](https://github.com/till213/SkyDolly/issues/197)).

---

## Fase 5 — Trasformazione in add-on del simulatore

Questa è la fase che cambia la natura del prodotto. Struttura finale, modellata su GSX:

```
Community/skydolly-panel/            ← pacchetto MSFS: icona toolbar + UI del pannello
%ProgramData%/SkyDolly/engine/       ← SkyDollyEngine.exe (servizio) + Qt DLL + plugin
EXE.xml                              ← voce Launch.Addon: MSFS avvia il servizio da solo
```

### 5a. Modalità servizio (l'app smette di essere un'app)

- **Nuova modalità headless.** `main.cpp:90-94` ha oggi un «simplistic command line parsing» in cui `argv[1]` è il percorso del logbook. Sostituirlo con un `QCommandLineParser` reale e aggiungere `--engine`: in questa modalità `MainWindow` non viene mai creata né mostrata, resta solo una **tray icon** (la `QSystemTrayIcon` esiste già, `MainWindow::createTrayIcon()`, `src/UserInterface/src/MainWindow.cpp:830-843`) con «Apri finestra», «Apri pannello», «Esci». `QApplication::setQuitOnLastWindowClosed(false)`.
- **Guardia di istanza singola** (`QLocalServer` o mutex con nome): oggi nulla impedisce a due istanze di aprire lo stesso logbook SQLite. Con l'avvio automatico da `EXE.xml` questo diventa una certezza, non un rischio. La seconda istanza deve passare il comando alla prima e uscire.
- Il servizio si connette a MSFS da solo e resta in attesa, con il backoff già esistente: all'avvio del sim è già pronto.

### 5b. Registrazione nel simulatore (installer)

- Comandi `SkyDollyEngine.exe --install-addon` / `--uninstall-addon`, implementati in C++ (niente secondo toolchain, e testabili), con un `Installa.bat` di comodo:
  1. rileva l'installazione MSFS 2024 (Steam `%APPDATA%\Microsoft Flight Simulator 2024`, MS Store `%LOCALAPPDATA%\Packages\Microsoft.Limitless_8wekyb3d8bbwe\LocalCache`) riusando la `FlightSimulator::isInstalled()` corretta nella Fase 2;
  2. copia `skydolly-panel/` nella cartella `Community`;
  3. aggiunge un blocco `<Launch.Addon>` a `EXE.xml` (stesso percorso di `SimConnect.xml`), creando il file se assente e **facendone un backup**; la disinstallazione rimuove solo la propria voce.
- Attenzione documentata: 2020 e 2024 hanno `EXE.xml` distinti, ed è l'errore più comune; e un aggiornamento del sim può resettare il file, quindi l'app deve accorgersi che la voce è sparita e riproporne l'aggiunta.

### 5c. Server locale nel servizio

Nuova libreria `src/Remote/` che linka `Qt6::Network` (da aggiungere ai componenti in `CMakeLists.txt:58`), con un server HTTP/1.1 minimale su `QTcpServer` associato **solo a 127.0.0.1**, porta configurabile:

| Endpoint | Uso |
|---|---|
| `GET /api/state` | snapshot JSON: stato, timestamp, durata, velocità, volo e aereo correnti, loop |
| `GET /api/events` | Server-Sent Events: push dei cambi di stato (supportato dal motore Coherent) |
| `POST /api/command` | `record`, `play`, `pause`, `stop`, `seek`, `skipBegin`/`skipEnd`, `forward`/`backward`, `speed`, `loop` |
| `GET /api/flights`, `POST /api/load` | elenco logbook recenti e caricamento volo |

Header `Access-Control-Allow-Origin: *` (il pannello ha origine `coui://`). Token opzionale in un file accanto all'exe. Il server parla **solo** con `SkyConnectManager` e `ModuleManager` — la facciata già esistente su cui si appoggiano `MainWindow` e i moduli — tramite invocazioni in coda sul thread principale.

### 5d. Pacchetto Community MSFS 2024 — la UI primaria

Nuova directory top-level `msfs/skydolly-panel/`, zippata dalla CI:

```
manifest.json                                     content_type "MISC"
layout.json                                       (rigenerato in fase di packaging)
html_ui/icons/toolbar/skydolly.svg                icona nella toolbar
InGamePanels/SkyDollyPanel/panel.cfg + .spb
html_ui/InGamePanels/SkyDollyPanel/SkyDollyPanel.{html,js,css}
```

Poiché il pannello è ora l'interfaccia principale e non un telecomando accessorio, deve coprire l'uso normale per intero:

- trasporto: registra / play / pausa / stop, timeline scrubabile, salto a inizio e fine, loop;
- velocità di replay e indicatore di stato (in registrazione, in replay, connesso);
- elenco dei voli recenti del logbook con caricamento di un volo;
- selezione della **modalità replay motori** della Fase 4, per aereo;
- stato esplicito «servizio Sky Dolly non in esecuzione», con istruzioni, quando il server non risponde.

Il pannello disegna i propri controlli nativamente (stesso approccio di addon 2024 funzionanti come *The SimBrief Panel*) e parla con `http://localhost:<porta>` via `fetch` + `EventSource`. Da tenere presente: il motore è **Coherent GT**, basato su una versione datata di Chromium — scrivere JS conservativo (niente sintassi recente) e verificare con il debugger Coherent su `http://127.0.0.1:19999`.

*Rischio dichiarato:* i pannelli in-game personalizzati non sono documentati ufficialmente nell'SDK — è una tecnica di community, usata però da GSX, Navigraph e FSLTL. Un Sim Update può romperla: da qui l'overlay di ripiego.

### 5e. Overlay e scorciatoie (ripiego)

- Mini-controller `QWidget` frameless, traslucido e always-on-top, riusando i widget di controllo della minimal UI già presenti (`MainWindow::updateMinimalUi`, `src/UserInterface/src/MainWindow.cpp:908-938`), attivabile con hotkey globale Windows (`RegisterHotKey` + `QAbstractNativeEventFilter`).
- Sistemare ed estendere le scorciatoie in-sim già funzionanti (`Event/InputEvent.cpp`), inclusa la collisione di flag della Fase 1.

---

## Fase 6 — Documentazione e chiusura

- Aggiornare `README.md`, `ABOUT.md`, `BUILD.md`, `RELEASE.md` per il target unico MSFS 2024 (oggi dicono esplicitamente 2020) e correggere `SECURITY.md`/`SHASUM256.md`, fermi a 0.19.x. La documentazione va riscritta attorno al nuovo modello d'uso: si installa una volta, poi si usa dalla toolbar del sim.
- L'artifact di release diventa un unico ZIP con dentro il servizio, il pacchetto `skydolly-panel/` e `Installa.bat` / `Disinstalla.bat`.

---

## Verifica

**Automatica (CI, ciò che posso davvero garantire da qui):**
- Il workflow Windows compila in Release e produce lo ZIP: è la verifica di compilazione, dato che io lavoro su macOS.
- `ctest` con `-DSKY_TESTS=ON`. Le suite esistenti sono `KernelTest`, `ModelTest`, `PluginManagerTest` e **nessuna tocca SimConnect**. Aggiungo test nuovi dove è possibile testare senza il sim:
  - `SkyMathTest` → SLERP quaternioni vs Hermite Euleriano su virate sintetiche;
  - `ModelTest` → `Attitude::interpolate` continuità attraverso il wrap 0/360 e attraverso i limiti della finestra di interpolazione;
  - nuovo `RemoteTest` → parsing delle richieste HTTP, serializzazione JSON dello stato, rifiuto delle connessioni non-loopback;
  - nuovo `InstallerTest` → inserimento e rimozione idempotente del blocco `Launch.Addon` in `EXE.xml` (file assente, file già contenente altri addon, voce già presente, XML malformato) — verificabile interamente su file di prova, senza simulatore;
  - `ModelTest` → decimazione adattiva della posizione (errore massimo rispetto alla traccia originale sotto soglia).
- Il plugin `PathCreator` (`src/Plugins/Connect/PathCreator/`) genera traiettorie sintetiche **senza simulatore**: lo uso per validare fine a fine la catena registrazione → persistenza → interpolazione → replay, inclusa la nuova frequenza di campionamento, prima ancora di toccare MSFS.

**Manuale, in MSFS 2024 (necessariamente da parte tua, con l'artifact della CI):**
0. Installazione: esegui `Installa.bat`, avvia MSFS 2024 → il servizio parte da solo, l'icona Sky Dolly compare nella toolbar, e non hai lanciato nulla a mano. `Disinstalla.bat` rimuove tutto senza lasciare residui in `EXE.xml`.
1. Connessione: avvio dal menu principale, ingresso in volo, ritorno al menu, chiusura del sim → nessun crash, riconnessione pulita, log presente, il servizio termina insieme al sim.
2. Fluidità: registra ~60 s con virate strette e riproduci — confronto affiancato con il video di [#184](https://github.com/till213/SkyDolly/issues/184).
3. Motori/suoni: avviamento a freddo + decollo su (a) un GA a pistoni, (b) un aereo nativo 2024, (c) un addon complesso; verifica che il suono segua la manetta e che nessun motore si spenga da solo. Confronto fra le tre modalità di replay motori.
4. Eliche/eVTOL: replay del Joby S4, eliche che girano e con il passo corretto.
5. Seek e loop: trascinamento sulla timeline avanti e indietro, salto a inizio/fine, loop attivo — nessuna deriva di quota, nessun riavvio spurio dei motori.
6. Teletrasporto lungo: carica un volo registrato dall'altra parte del mondo e premi Play → il sim non deve congelarsi.
7. Pannello in-game: icona presente in toolbar, controlli reattivi, stato aggiornato in tempo reale, elenco voli e caricamento funzionanti, comportamento corretto in VR, e messaggio chiaro se il servizio non è in esecuzione.
8. Sessione completa senza mai uscire dal simulatore: registra, ferma, ricarica e riproduci un volo usando solo il pannello.

---

## Note di rischio

- **Non posso compilare né provare nulla da qui.** La CI copre la compilazione; la verifica funzionale contro MSFS 2024 dipende interamente dai tuoi test. Consegnerò a fasi, con un artifact provabile alla fine di ciascuna.
- I submodule (`3rdParty/{cpptrace,geographiclib,ordered-map}`) non sono attualmente inizializzati in questo clone: primo passo della Fase 0.
- Le Fasi 3 e 4 cambiano il formato dei dati registrati (frequenza posizione, nuovi campi motore). I logbook esistenti restano leggibili grazie alle migrazioni, ma i **voli già registrati non guadagneranno** gli RPM mancanti né la posizione ad alta frequenza: il miglioramento si vede sulle registrazioni nuove.
- Il pannello in-game usa un'API non ufficiale (vedi 5d). L'overlay della 5e è la garanzia contro un Sim Update che la rompa.
- **Un processo in background resta necessario**: il codice che registra, interpola e scrive il logbook è C++/Qt/SQLite e non può girare come modulo WASM dentro al simulatore. La Fase 5 lo rende invisibile e auto-avviato, non lo elimina — è la stessa scelta che fa GSX con Couatl.
- `EXE.xml` è un file condiviso fra tutti gli add-on e viene talvolta azzerato dagli aggiornamenti del sim: da qui il backup, l'inserimento idempotente e il controllo all'avvio.

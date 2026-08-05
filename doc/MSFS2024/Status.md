# MSFS 2024 port — stato dei lavori

Documento di passaggio di consegne fra sessioni e fra macchine. Il piano completo è in
[Plan.md](Plan.md). Aggiornare questo file alla fine di ogni fase.

**Branch di lavoro:** `msfs2024` (partito da `main` @ `6b9b85ef`)

---

## A che punto siamo

| Fase | Contenuto | Stato |
| --- | --- | --- |
| 0 | Build riproducibile, `FindSimConnect`, CI Windows con packaging | **fatta e verificata su Windows** |
| 1 | Crash del plugin SimConnect | scritta, compila, **l'app si connette**; i percorsi di crash non ancora sollecitati |
| 1b | Log su file e crash handler Windows | scritta, **log verificato**; crash handler non ancora sollecitato |
| 2 | Targeting MSFS 2024 (rilevamento versione, pause, INITPOSITION, tempo) | **a metà** — vedi sotto |
| 3 | Allineamento e fluidità del replay | **scritta**, provata solo fuori dal simulatore |
| 4 | Fedeltà motori e suoni | da fare |
| 5 | Add-on del simulatore (servizio, installer, server locale, pannello) | **scritta**, provata fuori dal simulatore |
| 6 | Documentazione e release | da fare |

**La Fase 5 è stata anticipata su richiesta dell'utente**, prima delle Fasi 3 e 4: l'obiettivo era
poter usare Sky Dolly da dentro il gioco il prima possibile. Le Fasi 3 e 4 migliorano la *qualità*
del replay, non l'accesso, quindi non erano un prerequisito.

Dalla sessione del 2026-07-31 il progetto **compila, gira e si connette a MSFS 2024 su Windows**.
Restano non provate registrazione, replay, motori e tutto ciò che richiede di volare davvero.

---

## Cosa è stato fatto in concreto

### Fase 0
- `cmake/FindSimConnect.cmake` riscritto: cerca l'SDK 2024, poi il 2020, poi `3rdParty/SimConnect/`.
  Corretti `SimConnect_FOUND` mai impostato, un `if()` vuoto e un riferimento a variabile inesistente.
- `cmake/InitSubmodules.cmake`: il controllo sui submodule iterava la variabile sbagliata (codice
  morto) e usava `cmake/` come working directory invece della root.
- `.github/workflows/windows-release.yml`: nuova, produce un pacchetto verificato come artifact.
- `CMakeLists.txt`: aggiunta l'opzione `SKY_REQUIRE_SIMCONNECT` e il componente Qt `Network`.
- **Correzione `ctest`** (era il problema aperto n. 1, con diagnosi sbagliata): non c'entravano né
  i CRLF né le fixture. `3rdParty/geographiclib/tests/CMakeLists.txt` chiama `enable_testing()` e
  registra 189 test che pilotano i suoi tool da riga di comando (`GeoConvert`, `GeodSolve`,
  `Planimeter`, …); quei tool stanno in un `add_subdirectory(... EXCLUDE_FROM_ALL)` e non vengono
  mai compilati, quindi `ctest` li segnalava tutti «Not Run» e usciva con errore. Ora vengono
  disattivati in `CMakeLists.txt` subito dopo `add_subdirectory`, così `ctest` funziona anche dalla
  radice del build tree, come documentato in `CLAUDE.md` (la workflow preesistente `unit-tests.yml`
  aggirava il problema eseguendo `ctest` da `build/test`, che però salta in silenzio eventuali test
  registrati fuori da `test/`).
  Il `.gitattributes` che marca le fixture come binarie era stato aggiunto inseguendo l'ipotesi
  sbagliata dei CRLF: resta perché è comunque corretto — quelle fixture sono confrontate byte per
  byte — ma non era la causa di alcun problema.
- **Correzione del packaging**: la workflow eseguiva `windeployqt` sull'eseguibile e sui plugin in
  `bin/Plugins/`, ma `windeployqt` non segue le librerie proprie di Sky Dolly. `Qt6Sql.dll` — e con
  essa l'intera cartella `sqldrivers/` che serve al logbook — arriva **solo** da `Persistence.dll`,
  che sta in `bin/` e non fra i plugin. Il pacchetto sarebbe uscito senza driver SQLite e il passo
  «Verify package contents» avrebbe fallito *dopo* la pubblicazione. Ora scansiona ogni DLL in `bin/`.

### Fase 1 — i crash
- `SIMCONNECT_RECV_ID_QUIT` chiudeva e riapriva l'handle SimConnect **dentro la callback
  `SimConnect_CallDispatch` di quello stesso handle**. Ora differito sull'event loop
  (`handleSimulatorQuit`), come l'evento `Crashed` (`handleSimulatorCrashed`) e le scorciatoie
  (`deferActionActivated`).
- `AbstractSkyConnect::retryWithReconnect()` riapriva la connessione senza chiudere quella stantia:
  perdeva un handle e tutte le definizioni dati a ogni fallimento. Ora chiude prima, ritenta una
  volta, ed è sospeso durante il dispatch (`setReconnectSuspended`).
- Null-guard su `d->simConnectAi` e sull'handle; guard di re-entrancy (`d->dispatching`).
- `SIMCONNECT_RECV_ID_EXCEPTION` era gestita **solo in Debug**: in release ogni richiesta rifiutata
  da MSFS 2024 era invisibile. Ora decodificata in `SimConnectError.h` e loggata sempre.
- Difetti minori corretti: luce strobo che seguiva quella di rullaggio
  (`EventStateHandler.h`), collisione dei flag `Stop`/`Forward` (`InputEvent.cpp`), scorciatoie
  predefinite sfalsate di un membro (`ConnectPluginBaseSettings.cpp`), tre valori di ritorno
  scartati in `sendAircraftHandle()`, deref di un cast potenzialmente nullo in
  `SkyConnectManager::getCurrentSkyConnect()`.

### Fase 1b — diagnostica
- `Kernel/Log.{h,cpp}`: log su file con rotazione. Il percorso reale è
  `%LOCALAPPDATA%\till213\Sky Dolly\logs\` (`QStandardPaths::AppLocalDataLocation`, che include
  organizzazione e nome applicazione), **non** `%LOCALAPPDATA%\SkyDolly` come indicato in
  precedenza in questo documento. Verificato: il file viene creato e scritto.
- `SkyDolly/src/CrashHandler_Windows.cpp`: `SetUnhandledExceptionFilter` + minidump + stack trace
  in `…\Sky Dolly\crash\`. Non ancora sollecitato (nessun crash finora).

### Fase 2 — targeting MSFS 2024 (parziale)
Fatto:
- **Compilazione contro l'SDK 2024** verificata: `FindSimConnect` trova `C:\MSFS 2024 SDK`
  (SDK 1.6.9) e `MSFSSimConnect.dll` viene linkata contro quella `SimConnect.lib`.
- **Rilevamento della versione a runtime.** Nuovo tipo `Kernel/SimulatorVersion.{h,cpp}`, popolato
  in `SIMCONNECT_RECV_ID_OPEN` dal payload che finora veniva solo stampato. Esposto da
  `SkyConnectIntf` → `AbstractSkyConnect` → `SkyConnectManager` e mostrato nella dialog About
  (il testo della About è copiabile negli appunti: è il posto giusto per un bug report).
  Dato reale osservato: MSFS 2024 si identifica come **`SunRise`, versione applicazione 12.2,
  build 282174.999, SimConnect 12.2** (MSFS 2020 si identifica come `KittyHawk`, versione 11.x).
  Il riconoscimento usa il nome, con la versione major ≥ 12 come ripiego se il nome cambiasse.
- **`FlightSimulator`**: aggiunto `Id::MSFS2024`. `isRunning()` cerca `FlightSimulator2024.exe`.
  `isInstalled()` ora **onora il proprio argomento**: prima lo ignorava e rispondeva sempre per il
  2020, per giunta con un percorso impossibile (`%APPDATA%` finisce già in `Roaming`, quindi
  `%APPDATA%/Local/Packages/…` non esiste su nessuna macchina). Percorsi corretti per Steam e MS
  Store, per 2024 e 2020, più Prepar3D v5. Rimossa la dichiarazione morta `isMSFSInstalled()`.
- `MSFSSimConnectPlugin.json` dichiara `"flightSimulator": "MSFS2024"`;
  `FlightSimulator::nameToId()` lo riconosce; il ripiego in `SkyConnectManager::initialisePlugin()`
  prova prima il 2024 e poi il 2020; i commenti di `res/SimConnect.cfg` documentano i percorsi
  `SimConnect.xml` di entrambe le edizioni di entrambe le versioni.

Da fare (il resto della Fase 2):
- Semantica **pause/stato** del 2024 e macchina a stati «sono davvero in volo» ([#186]).
- **Teletrasporto**: sostituire `SIMCONNECT_DATA_INITPOSITION` su lunghe distanze.
- **Tempo di replay**: verificare se `ZULU_*_SET` funziona ancora, altrimenti disattivare la
  funzione con un messaggio esplicito.
- Esporre in UI lo stato reale della connessione invece di ritentare in silenzio.

[#186]: https://github.com/till213/SkyDolly/issues/186

### Fase 5 — add-on del simulatore (anticipata)
- **`src/Remote/`**, nuova libreria: server HTTP/1.1 minimale su `QTcpServer`, in ascolto **solo**
  su loopback e che per giunta rifiuta ogni peer non-loopback. Endpoint `GET /api/state`,
  `GET /api/events` (Server-Sent Events), `POST /api/command`, `GET /api/flights`, `POST /api/load`.
  Gira nell'event loop dell'applicazione, non in un thread: nessun lock, nessun comando applicato a
  metà. Token di accesso opzionale.
  - Lo stream SSE spinge sui cambi di stato, ma il timestamp cambia a ogni frame del simulatore:
    inoltrarlo significherebbe ~60 messaggi al secondo dentro il browser del sim. Un timer da 250 ms,
    attivo solo se qualcuno ascolta, tiene la timeline fluida.
  - `POST /api/load` risponde 409 se una registrazione o un replay è in corso: ricaricare il volo
    corrente sotto il plugin lo lascerebbe a inviare dati di un volo che non esiste più.
- **`msfs/skydolly-panel/`**, pacchetto Community: icona in toolbar e pannello con trasporto,
  timeline scrubabile, velocità, elenco dei voli recenti e stato esplicito «Sky Dolly non in
  esecuzione». JavaScript conservativo per Coherent GT (niente arrow function, template literal,
  `const`/`let`, `async`/`await`).
- **Lo stesso pannello è servito su `http://127.0.0.1:17285/`**, incorporato come risorsa Qt a
  partire dagli stessi file del pacchetto Community: unica fonte di verità, i due non possono
  divergere. Serve sia per sviluppare e provare senza lanciare il simulatore, sia come ripiego se un
  Sim Update rompe il pannello in-game.
- **Modalità servizio**: `QCommandLineParser` vero al posto del «simplistic command line parsing»
  (`argv[1]` come percorso del logbook), più `--engine` (nessuna finestra, solo tray, con voce
  «Open Sky Dolly»), `--port`, `--no-panel-server`. Guardia di istanza singola su `QLocalServer`:
  il secondo processo passa la riga di comando al primo, lo fa venire in primo piano ed esce.
- **`--install-addon` / `--uninstall-addon`** (`Kernel/AddonInstaller`): copia il pacchetto nella
  `Community` e aggiunge una voce `Launch.Addon` a `EXE.xml`. `EXE.xml` è condiviso con tutti gli
  altri add-on: backup prima di toccarlo, si riscrive solo la voce il cui `Name` è la nostra,
  l'aggiunta è idempotente, e un file che non si parsifica viene **rifiutato** e lasciato intatto
  (sostituirlo cancellerebbe in silenzio la registrazione di un altro add-on). La `Community` non è
  a un percorso fisso: si legge `InstalledPackagesPath` da `UserCfg.opt`.
  Script di comodo `msfs/Install.bat` e `msfs/Uninstall.bat`.

---

## Verifiche fatte davvero (2026-07-31, prima sessione su Windows)

- `cmake` configure + build Release: **535/535 target, zero errori, zero warning**.
- `ctest`: **12/12, exit 0**.
- L'applicazione parte, resta responsiva e **si connette a MSFS 2024 in esecuzione**. Riga di log:
  `SimConnect: connected to "Microsoft Flight Simulator 2024 12.2 (build 282174.999, SunRise, SimConnect 12.2)"`
- Rilevamento simulatore verificato con una sonda che linka la `Kernel` compilata, su una macchina
  con **solo** MSFS 2024 (edizione Steam) installato e in esecuzione:

  | | `isRunning` | `isInstalled` |
  | --- | --- | --- |
  | `MSFS2024` | sì | sì |
  | `MSFS` (2020) | no | no |
  | `Prepar3Dv5` | no | no |
  | `All` | no | sì |

  Prima di queste modifiche entrambe le colonne rispondevano «no» per MSFS 2024.

### Fase 3 — allineamento e fluidità del replay
- **Posizione registrata a ogni frame** invece che a 1 Hz (`updateRequestPeriod`). Era la causa
  vera dello stutter di [#184]: a velocità di crociera un secondo sono centinaia di metri, e la
  spline doveva inventarsi tutto il percorso in mezzo mentre l'assetto accanto era campionato 60
  volte tanto. Piano di volo e tempo di simulazione restano a 1 Hz, che è corretto.
- **Decimazione adattiva in scrittura** (`Model/PositionDecimation`): si conserva un campione solo
  se l'aereo è altrove rispetto a dove i due precedenti lo prevedevano, oltre mezzo metro, oppure
  se è passato un secondo comunque. Dieci minuti di crociera sono 36 000 campioni e diventano meno
  di 1200 righe; su una virata in salita la traccia decimata resta entro 5 metri da quella volata.
- **Interpolazione dell'assetto su quaternioni** (`SkyMath::slerp` / `squad`, usata da
  `Attitude::interpolate`). Va detto onestamente: misurata contro il codice precedente vale **meno
  di un grado** (0.08° in virata dolce, 0.88° con campioni radi) e **zero** attraverso il wrap
  0/360, che `interpolateHermite360` già gestiva. È una correzione di correttezza, non la causa
  dello stutter. Ha però fatto emergere un bug reale: attraversando nord i quaternioni finivano in
  emisferi opposti e i punti di controllo di `squad` producevano salti fino a 8°.
- **Integratore ASRA azzerato e limitato**: `currentAltitudeOffset` accumulava per tutta la
  sessione attraverso seek, riavvii e riconnessioni, senza reset né limite. Ora azzerato su
  start/stop/seek e limitato a 50 piedi. Candidato diretto per [#155].
- **Orologio di replay letto al frame**, non a ogni messaggio Windows: prima più frame nello stesso
  dispatch venivano riprodotti allo stesso timestamp.

Non fatto della Fase 3: collegare `SampleRate` all'option widget del plugin (la frequenza è quella
dei frame, non configurabile), inserimenti in batch in `SQLiteAircraftDao`, e la ricostruzione dello
stato motori sul seek senza rieseguire l'avviamento.

[#155]: https://github.com/till213/SkyDolly/issues/155
[#184]: https://github.com/till213/SkyDolly/issues/184

### Fase 5, verificata fuori dal simulatore
- `ctest`: **14/14, exit 0** (aggiunti `HttpRequestTest` e `AddonInstallerTest`).
- API provata contro MSFS 2024 in esecuzione: `/api/state` riporta il simulatore connesso,
  `/api/flights` elenca i voli reali del logbook dal più recente, un comando risponde con lo stato
  risultante, comando ed endpoint sconosciuti rispondono 400 e 404 invece di chiudere la connessione.
- Pannello aperto in un browser su `http://127.0.0.1:17285/`: mostra il simulatore connesso, elenca
  i 4 voli del logbook con aereo e località, e **cliccandone uno lo carica** — titolo, aereo, durata
  9:36 e timeline attiva tornano indietro lungo tutta la catena. I pulsanti si abilitano e
  disabilitano correttamente.
- `--engine`: nessuna finestra principale, API comunque raggiungibile. Secondo avvio: esce con
  codice 0, resta un solo processo, e la finestra del primo viene in primo piano.
- Percorsi dell'installer verificati **in sola lettura** sull'installazione reale: `UserCfg.opt`
  porta a una `Community` esistente, `EXE.xml` non esiste ancora.

**Non ancora provato**: il pannello **dentro** il simulatore (vedi problemi aperti), registrazione,
replay, motori, suoni, seek, teletrasporto, uscita al menu
principale e chiusura del simulatore (cioè i percorsi che la Fase 1 doveva rendere sicuri).

---

## Problemi aperti

1. **`3rdParty/SimConnect/` è vuota.** Senza i tre file dell'SDK la CI salta il plugin di
   connessione e non produce il pacchetto. Istruzioni in `3rdParty/SimConnect/README.md`.
   *Prima di riempirla, sciogliere il punto 2.*
2. **La licenza di SimConnect va verificata.** `THIRD_PARTY.md` dichiara «MIT License», ma l'SDK
   2024 installato contiene soltanto `C:\MSFS 2024 SDK\Licenses\MSFS SDK EULA.pdf` — non una
   licenza MIT. Committare `SimConnect.lib` e `SimConnect.dll` in un repository pubblico è una
   decisione da prendere in modo consapevole: la ridistribuzione della sola DLL insieme
   all'applicazione (che il progetto originale fa da sempre) è un caso diverso dal committare la
   libreria nel repo. In alternativa la CI può scaricare l'SDK da un artifact privato oppure
   girare su un runner self-hosted con l'SDK installato.
3. Su questa macchina la build locale non è vincolata a `3rdParty/SimConnect/`: `FindSimConnect`
   trova l'SDK installato.
4. **La registrazione del pannello nella toolbar non è confermata.** I pannelli in-game di terze
   parti non sono documentati nell'SDK: `SkyDollyPanel.xml` segue quello che sembrano fare gli
   add-on funzionanti, ma **non è ancora stato caricato da MSFS 2024**. Se l'icona non compare:
   controllare che `layout.json` corrisponda ai file su disco, e aprire il debugger Coherent su
   `http://127.0.0.1:19999` (con Developer Mode attivo) che elenca i pannelli davvero caricati.
   Nel frattempo `http://127.0.0.1:17285/` funziona e offre la stessa identica interfaccia.
   Dettagli in [`msfs/README.md`](../../msfs/README.md).

---

## Smart App Control

Su questa macchina **Smart App Control è attivo** (`VerifiedAndReputablePolicyState = 1`).
Blocca i binari appena linkati, perché non hanno reputazione nell'Intelligent Security Graph: per
qualche minuto dopo la compilazione i test non partono e l'applicazione non riesce a caricare le
proprie DLL (evento CodeIntegrity 3077). **Poi il blocco decade da solo** e tutto funziona.

Quindi non serve disattivarlo — cosa peraltro irreversibile senza reinstallare Windows. Se subito
dopo una build i test falliscono con `0xc0e90002` o `BAD_COMMAND`, aspettare e rieseguire.

Nota per la Fase 6: la release ufficiale di Sky Dolly **non è firmata** (solo le DLL di Qt lo sono)
e gira lo stesso, perché quei byte esatti hanno reputazione. Un pacchetto nuovo prodotto da noi non
l'avrà: per distribuirlo servirà un certificato di code signing.

## Ambiente di sviluppo su questa macchina

Installato il 2026-07-31 (prima non c'era alcuna toolchain, solo git):

| Componente | Versione | Percorso |
| --- | --- | --- |
| MSFS 2024 SDK | 1.6.9 | `C:\MSFS 2024 SDK` |
| VS Build Tools 2022 | MSVC 14.44 / cl 19.44 | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` |
| Windows SDK | 10.0.26100 | |
| CMake | 4.4.1 | `C:\Program Files\CMake\bin` |
| Ninja | 1.12+ | via winget |
| Qt | 6.8.0 `msvc2022_64` | `C:\Qt\6.8.0\msvc2022_64` |

MSFS 2024 è l'edizione **Steam**: `C:\Program Files (x86)\Steam\steamapps\common\MSFS2024\FlightSimulator2024.exe`,
dati utente in `%APPDATA%\Microsoft Flight Simulator 2024`.

`cl` non è nel `PATH` di default: serve `vcvars64.bat`. Sequenza completa in una shell `cmd`:

```
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set PATH=C:\Program Files\CMake\bin;C:\Qt\6.8.0\msvc2022_64\bin;%PATH%
set CMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSKY_REQUIRE_SIMCONNECT=ON -DSKY_FETCH_EGM=ON -DSKY_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Per eseguire l'app dalla cartella di build serve il runtime Qt accanto all'eseguibile
(`windeployqt --release build\bin\SkyDolly.exe`, poi lo stesso su ogni DLL in `build\bin`, come fa
la CI): altrimenti manca `Qt6Sql.dll` e il logbook non si apre.

## Note

- **`ctest` va eseguito da `build/test`, non dalla radice del build tree.** Alla radice sono
  registrati anche i ~190 test di GeographicLib, i cui strumenti a riga di comando sono
  `EXCLUDE_FROM_ALL` e quindi non vengono mai compilati: ctest li riporta come "Not Run" e
  fallisce. È il motivo per cui la workflow preesistente `unit-tests.yml` fa `cd build/test`.
- Il `.gitattributes` che marca le fixture come binarie è stato aggiunto inseguendo un'ipotesi
  sbagliata (conversione CRLF). Resta perché è comunque corretto — quelle fixture sono confrontate
  byte per byte — ma non era la causa di alcun problema.

---

## Cosa serve dall'utente

- **Installare l'add-on e dire se l'icona compare in toolbar.** Dalla cartella di build:
  `build\bin\SkyDolly.exe --install-addon`, poi riavviare MSFS 2024. È l'unica verifica che non si
  può fare da qui, ed è quella che dice se la Fase 5 è davvero finita.
- **Tutte le prove dentro il simulatore**: registrazione, replay, motori, suoni, pannello. In
  particolare, per chiudere la Fase 1, i tre passaggi che prima facevano crashare: entrare in volo,
  tornare al menu principale, chiudere il simulatore mentre Sky Dolly è connesso.
- La decisione sul punto 2 dei problemi aperti (licenza / vendorizzazione di SimConnect).

---

## Ripartire da una macchina nuova

```
git clone --recurse-submodules -b msfs2024 https://github.com/EdoWhite/SkyDolly24.git
```

Il `--recurse-submodules` è necessario: senza `3rdParty/{cpptrace,geographiclib,ordered-map}`
CMake non configura.

Poi, in una sessione di Claude Code aperta nella cartella del progetto, è sufficiente chiedere di
leggere questo file e `Plan.md` e proseguire dalla prima fase non completata.

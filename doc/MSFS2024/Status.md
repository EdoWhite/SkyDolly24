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
| 3 | Allineamento e fluidità del replay | **scritta**; ASRA corretta dopo la prova in volo |
| 4 | Fedeltà motori e suoni | da fare — **il pezzo più grosso rimasto** |
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

## Prima prova in volo (2026-08-05) — risultati

L'add-on è stato installato e provato dentro MSFS 2024. `EXE.xml` funziona: **il simulatore avvia
Sky Dolly da solo** con `--engine`. Il pannello in toolbar **non compare** (vedi problemi aperti).
Il replay è stato giudicato funzionante, senza segnalazione di scatti.

Tre difetti osservati, in ordine di gravità:

1. **Sky Dolly crasha premendo Stop.** Access violation `0xc0000005`. **Analizzato e risolto: non
   era un difetto di Sky Dolly.** Vedi la sezione seguente.
2. **Un secondo dopo è crashato anche MSFS.** Modulo in fallimento: `RTSSHooks64.dll_unloaded`,
   cioè **RivaTuner Statistics Server** (overlay di MSI Afterburner). Stessa causa del punto 1.
3. **Le ruote sprofondano nell'asfalto e l'atterraggio dà un «colpo».** Il plugin provato
   (10:01:55) **conteneva già** la correzione ASRA (commit delle 10:00:38), quindi reset e limite
   **non sono bastati**. Corretto: vedi «Correzione ASRA» sotto.

Motori e suoni fuori fase: atteso, è la Fase 4 (non registriamo alcun RPM/N1).

---

## Il crash dello Stop: era RivaTuner, non Sky Dolly (2026-08-05)

Il minidump è stato analizzato leggendolo direttamente (i `.pdb` non esistevano: la build Release
era `/O2 /Ob2 /DNDEBUG`, senza alcuna informazione di debug — vedi sotto). Il risultato non lascia
margini di dubbio.

**Non è un salto a `0x0`.** Il report diceva `0x0` soltanto perché lo stack trace era sbagliato (vedi
oltre). Il record dell'eccezione dice altro:

| | |
| --- | --- |
| Codice | `0xc0000005`, parametri `8`, `0x180071150` → violazione di **esecuzione** (DEP) |
| `RIP` | `0x0000000180071150` — **dentro nessun modulo caricato** |
| `RAX` = `RSI` | `0x180071150` — il puntatore chiamato |
| `RCX` / `RDX` / `R8` / `R9` | `0x10`, `0`, `0`, `0` |
| `RBX` | `winmm.dll+0x2aee0` |
| `[RSP]` | `winmm.dll+0x1f04` — l'indirizzo di ritorno |

Il thread che ha fallito è un **thread di callback dei timer multimediali di `winmm`**
(`ntdll!RtlUserThreadStart` → `kernel32!BaseThreadInitThunk` → `winmm+0xff92` → `winmm+0x283b` →
`winmm+0x1f04`). Il thread principale era **fermo nell'event loop di Qt**: nessun frame di Sky Dolly
sullo stack, da nessuna parte.

Disassemblando `winmm.dll` attorno a `+0x1f04` si vede esattamente il dispatch del timer:

```
180001E5F: lea  rax,[18002AEE0h]   ; tabella dei timer   <- RBX al crash = winmm+0x2aee0
180001E66: and  ebx,0Fh            ; slot = id & 15      (id=0x10 -> slot 0)
180001ED7: mov  rsi,[rbx+8]        ; il puntatore alla callback
180001EDB: call [LeaveCriticalSection]
180001EFC: mov  rax,rsi
180001EFF: call <thunk> -> jmp rax
180001F04:                         ; <- esattamente [RSP] nel dump
```

**A chi appartiene `0x180071150`:**

- `C:\Program Files (x86)\RivaTuner Statistics Server\RTSSHooks64.dll` ha `image base`
  **`0x180000000`**, `size of image` `0x267000`, e — decisivo — **`DllCharacteristics = 0x0120`,
  cioè senza `DYNAMICBASE`: niente ASLR**. Quella DLL si carica *sempre* al suo indirizzo preferito,
  quindi `0x180071150` è il suo indirizzo reale a runtime.
- Nella sua `.pdata`, `0x71150` è un **punto di ingresso di funzione esatto** (`[0x71150, 0x711ce)`,
  126 byte — la taglia di una callback di timer).
- Importa `timeSetEvent` e `timeKillEvent` da `WINMM.dll`.
- Qt6Core è escluso: **è** compilata con ASLR (`0x4160`), era caricata a `0x7ffe99190000`, e a
  `0x71150` non ha un inizio di funzione ma il mezzo di una.

Fra tutti i moduli caricati nel processo, **solo Qt6Core importa `timeSetEvent`** — e non è lei.

**Quindi:** RivaTuner inietta `RTSSHooks64.dll` in SkyDolly.exe, registra un timer multimediale
(id 16, `dwUser` 0), e poi **si scarica senza chiamare `timeKillEvent`**. Al tick successivo il
thread dei timer di `winmm` chiama codice che non è più mappato → violazione di esecuzione. Il
filtro `SetUnhandledExceptionFilter` di Sky Dolly, che è per processo e non per thread, ha
raccolto il crash di qualcun altro e se n'è preso la colpa. È lo **stesso difetto della stessa DLL**
che un secondo dopo ha fatto cadere MSFS (`RTSSHooks64.dll_unloaded`).

**Per l'utente:** non c'è niente da correggere in Sky Dolly per questo crash. Per non rivederlo,
disattivare l'overlay RTSS (o escludere `SkyDolly.exe` e `FlightSimulator2024.exe` dai suoi
profili). È anche la misura (b) chiesta per il difetto degli scatti, quindi una prova sola risponde
a due domande.

### Cosa è stato comunque corretto, grazie a questo crash

- **Il freeze dell'aereo si scioglie per primo** nella sequenza di stop (`AbstractSkyConnect::
  stopReplay`), prima di qualunque passo che possa fallire. Era l'ultima cosa, dopo il teardown del
  plugin: se quello falliva, l'aereo restava immobilizzato per tutto il volo.
- **Il crash report ora descrive il guasto vero.** Prima chiamava `StackTrace::generate()`, che
  percorre lo stack di *chi la chiama* — cioè il filtro delle eccezioni — e non quello del thread
  che ha fallito: nove frame di `UnhandledExceptionFilter` e `KiUserExceptionDispatcher`, più un
  `0x0` inventato. Ora percorre il `CONTEXT` catturato al guasto con `StackWalk64`, e aggiunge
  l'indirizzo che ha fallito, **il modulo che lo possiede**, il tipo di accesso (lettura, scrittura
  o esecuzione) e il thread. Quando nessun modulo caricato possiede quell'indirizzo lo dice
  esplicitamente — che è già una diagnosi: puntatore stantio, o DLL scaricata senza annullare una
  callback. Questa riga sola avrebbe risposto in un secondo.
- **La build Release produce i `.pdb`** (`/Z7` più `/DEBUG /OPT:REF /OPT:ICF`; `/Z7` e non `/Zi` per
  non serializzare la build parallela attraverso `mspdbsrv`, e gli `/OPT:` espliciti perché
  `/DEBUG` altrimenti li spegne e cambierebbe il layout dei binari). La CI li pubblica come
  artifact separato: sono ciò che trasforma il report di un utente in nomi di funzione, e devono
  essere quelli della build esatta che sta usando. Lo strip dal pacchetto ora è ricorsivo — ogni
  plugin ha il suo, in una sotto-cartella, che il filtro non ricorsivo si lasciava dietro.

### Correzione ASRA

Il difetto era nella *forma* della correzione, non nella sua contabilità. A ogni frame a terra:
`offset -= misura`, cioè **guadagno d'anello pari a uno** attorno a un anello che ha uno o due frame
di ritardo di trasporto (l'altitudine inviata ora la vede il sensore uno o due frame dopo). Un
guadagno di uno con del tempo morto nell'anello **non si assesta: oscilla**, e contro il limite di
±50 piedi l'oscillazione diventava un bang-bang che piantava l'aereo nell'asfalto. Il limite non la
fermava, decideva solo quanto in profondità.

Ora è una **correzione proporzionale smorzata**: toglie circa il 15% dell'errore residuo per frame,
si assesta in una ventina di frame (un terzo di secondo) in modo graduale invece che in un salto —
ed è questo che toglie il colpo all'atterraggio — e con guadagno ben sotto uno è stabile nonostante
il ritardo. Una banda morta evita che il rumore del sensore faccia tremare un aereo fermo, e il
limite resta come rete di sicurezza.

In più: la correzione veniva applicata **anche dopo il decollo**, perché era aggiornata solo a terra
ma non azzerata mai, quindi un offset guadagnato in pista veniva portato fino a destinazione. Ora
svanisce una volta in volo. Lo svanimento è fatto sul frame di replay e non sul campione del
sensore, perché il sensore è richiesto con `SIMCONNECT_DATA_REQUEST_FLAG_CHANGED` e tace appena la
lettura si stabilizza.

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
4. **Il pannello in toolbar: causa individuata, serve l'SDK completo.** Confrontando il nostro
   pacchetto con **tutti** i pannelli di Asobo installati (`fs-base-ingamepanels-*`, venti
   pacchetti) la differenza è netta e sempre la stessa:

   ```
   fs-base-ingamepanels-metar/
     InGamePanels/InGamePanel_Metar.spb          <-- LA REGISTRAZIONE
     html_ui/InGamePanels/Metar/MetarPanel.{html,css,js}
     layout.json, manifest.json

   skydolly-panel/                               <-- il nostro
     html_ui/InGamePanels/SkyDollyPanel/SkyDollyPanel.{html,css,js,xml}
     layout.json, manifest.json
                                                 <-- manca del tutto InGamePanels/*.spb
   ```

   Il descrittore è **`InGamePanels/InGamePanel_<Nome>.spb`**, un binario AceXML. Il nostro
   `html_ui/.../SkyDollyPanel.xml` **non è il meccanismo**: nessuno dei venti pacchetti di Asobo ha
   un file simile, e il simulatore non lo guarda. Senza `.spb` non c'è voce in toolbar, punto.

   Il `.spb` **non è scrivibile a mano**: le stringhe al suo interno sono offuscate, e i nomi degli
   elementi AceXML non compaiono nemmeno come stringhe in chiaro dentro `FlightSimulator2024.exe`
   (solo `SimBase.Document` e `AceXML Document`), il che vuol dire che il loader confronta forme
   già offuscate. Va compilato da un sorgente XML con
   `C:\MSFS 2024 SDK\Tools\bin\fspackagetool.exe`.

   **Cosa manca per farlo:** lo schema del sorgente `InGamePanel_*.xml`. L'SDK installato su questa
   macchina è **parziale** — `C:\MSFS 2024 SDK` contiene solo `Licenses`, `LodProcessingPresets`,
   `ModelBehaviorDefs`, `Schemas`, `SharedAssets`, `SimConnect SDK`, `Tools`, `WASM`: **niente
   `Samples`, niente `Documentation`**, e in `Schemas` ci sono solo schemi glTF. Su tutta la
   macchina non esiste un solo `InGamePanel*.xml`. Reinstallare l'SDK includendo Samples e
   Documentation dovrebbe fornire sia lo schema sia un progetto di esempio da copiare; a quel punto
   la compilazione è un comando solo. Nota: `fspackagetool` costruisce **lanciando
   `FlightSimulator2024.exe`** in modalità build, quindi non è un passo silenzioso.

   Nel frattempo `http://127.0.0.1:17285/` funziona e offre la stessa identica interfaccia.
   Dettagli in [`msfs/README.md`](../../msfs/README.md).

---

## Reggerà ai Sim Update?

Dipende dal pezzo, e la differenza è netta.

**Regge (rischio basso).** Tutto ciò che è *funzione* — registrazione, replay, seek, freeze,
posizione, luci, comandi di volo — passa da **SimConnect**, che è un'API versionata e mantenuta
compatibile all'indietro: gira ancora oggi software scritto per FSX. Un Sim Update non la rompe.
Lo stesso vale per l'avvio automatico via `EXE.xml`, che è documentato e non è cambiato fra MSFS
2020 e 2024, e per il rilevamento della versione, che usa il nome (`SunRise`) con la major ≥ 12
come ripiego proprio perché la build cambia a ogni update.

**Rischio medio: le singole variabili ed eventi.** Nomi come `PLANE ALT ABOVE GROUND MINUS CG` o
`FREEZE_ALTITUDE_SET` sono stabili da anni, ma Asobo ogni tanto deprecia o rinomina qualcosa. Se
succede, si rompe *quella* funzione, non l'applicazione: dalla Fase 1 le eccezioni SimConnect sono
decodificate e finiscono nel log anche in release, quindi si vede subito quale richiesta è stata
rifiutata invece di avere un comportamento silenziosamente sbagliato. `ZULU_*_SET` è già sulla
lista delle cose da verificare sul 2024.

**Rischio alto: il pannello in toolbar.** È l'unico pezzo costruito su una tecnica **non
documentata**: un binario `.spb` con stringhe offuscate, in un formato proprietario che Asobo può
cambiare da un update all'altro senza dirlo a nessuno. Un Sim Update *può* far sparire l'icona.

Questo è esattamente il motivo per cui la stessa identica interfaccia è servita anche su
**`http://127.0.0.1:17285/`**, dagli stessi file: quella non dipende da nulla del simulatore —
è una pagina servita dal nostro processo a un browser — e nessun Sim Update la può toccare. Se un
giorno l'icona sparisce, l'add-on continua a funzionare e si perde solo la comodità di averlo
dentro al gioco.

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

## Fase 4 — motori e suoni: cosa manca davvero

`Model/EngineData` registra **solo posizioni di leve e interruttori**: manetta, elica, miscela,
cowl flap, batteria, avviamento, combustione. Non c'è **nessun giro motore**: né `GENERAL ENG RPM`,
né `TURB ENG N1`/`N2`, né temperature o pressioni. In replay il simulatore riceve le leve e ricalcola
i motori con il proprio modello, che parte da uno stato diverso da quello registrato: da qui i
motori e i suoni fuori fase.

Il lavoro si divide in due metà, molto diverse fra loro:

1. **Registrare** (dritta, verificabile fuori dal simulatore): aggiungere i giri a `EngineData`, ai
   sotto-record SimConnect (`SimConnectEngineCore`/`All`/`Ai`/`Event`), alle colonne del logbook con
   il marcatore `@migr` in coda a `LogbookMigration.sql`, a `SQLiteEngineDao`, e ai plugin di
   import/export che toccano i motori. Quelle variabili sono in sola lettura per noi: si leggono e
   basta.
2. **Riprodurre** (la parte difficile, richiede prove *dentro* il simulatore): in MSFS i giri motore
   **non sono scrivibili** come le posizioni delle leve — sono un'uscita del modello motore, non un
   ingresso. Prima di scrivere codice serve stabilire sperimentalmente **quali variabili motore il
   2024 accetta in scrittura** (`TURB ENG N1` indicizzata? `ENG ROTOR RPM`? `RECIP ENG RPM`?), e
   cosa succede combinandole con le leve già inviate. È una fase di scoperta, non di
   implementazione: scriverla adesso sulla base di variabili che potrebbero non essere scrivibili
   vorrebbe dire consegnare codice che non si può provare.

Va anche affrontato il punto già annotato nella Fase 3: ricostruire lo stato dei motori dopo un seek
senza rieseguire l'avviamento.

## Cosa serve dall'utente

- **Disattivare l'overlay di RivaTuner** (RTSS) e rifare la prova. Risponde a due domande in una:
  conferma la diagnosi del crash, ed è la misura (b) per gli scatti.
- **Le due misure per gli scatti**, prima di toccare il campionamento:
  (a) gli scatti ci sono anche con Sky Dolly **chiuso del tutto**?
  (b) ci sono anche con l'overlay RTSS **disattivato**?
  In replay il carico non è mai cambiato, quindi la Fase 3 probabilmente non c'entra.
- **Reinstallare l'SDK di MSFS 2024 includendo Samples e Documentation**, che l'installazione
  attuale non ha. Senza, lo schema del sorgente `InGamePanel_*.xml` non è ricavabile e l'icona in
  toolbar non si può compilare (vedi problema aperto n. 4).
- **Le prove dentro il simulatore che restano**: registrazione, motori, suoni, seek, teletrasporto,
  e — per chiudere la Fase 1 — tornare al menu principale e chiudere il simulatore mentre Sky Dolly
  è connesso.
- **Per la Fase 4**: stabilire quali variabili motore MSFS 2024 accetta in scrittura (vedi sopra).
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

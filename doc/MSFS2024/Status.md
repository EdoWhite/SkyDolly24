# MSFS 2024 port — stato dei lavori

Documento di passaggio di consegne fra sessioni e fra macchine. Il piano completo è in
[Plan.md](Plan.md). Aggiornare questo file alla fine di ogni fase.

**Branch di lavoro:** `msfs2024` (partito da `main` @ `6b9b85ef`)

---

## A che punto siamo

| Fase | Contenuto | Stato |
| --- | --- | --- |
| 0 | Build riproducibile, `FindSimConnect`, CI Windows con packaging | scritta, **compila** |
| 1 | Crash del plugin SimConnect | scritta, **compila**, non provata nel sim |
| 1b | Log su file e crash handler Windows | scritta, **compila**, non provata nel sim |
| 2 | Targeting MSFS 2024 (rilevamento versione, pause, INITPOSITION, tempo) | da fare |
| 3 | Allineamento e fluidità del replay | da fare |
| 4 | Fedeltà motori e suoni | da fare |
| 5 | Add-on del simulatore (servizio, installer, server locale, pannello) | da fare |
| 6 | Documentazione e release | da fare |

**Nulla è ancora stato provato dentro MSFS.** «Compila» significa soltanto che la CI Windows
(MSVC + Ninja + Qt 6.8) supera `Configure` e `Build`.

---

## Cosa è stato fatto in concreto

### Fase 0
- `cmake/FindSimConnect.cmake` riscritto: cerca l'SDK 2024, poi il 2020, poi `3rdParty/SimConnect/`.
  Corretti `SimConnect_FOUND` mai impostato, un `if()` vuoto e un riferimento a variabile inesistente.
- `cmake/InitSubmodules.cmake`: il controllo sui submodule iterava la variabile sbagliata (codice
  morto) e usava `cmake/` come working directory invece della root.
- `.github/workflows/windows-release.yml`: nuova, produce un pacchetto verificato come artifact.
  Prima non esisteva alcuna automazione di packaging (`RELEASE.md` era una checklist manuale).
- `CMakeLists.txt`: aggiunta l'opzione `SKY_REQUIRE_SIMCONNECT` e il componente Qt `Network`
  (serve alla Fase 5).

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
- `Kernel/Log.{h,cpp}`: log su file con rotazione sotto `%LOCALAPPDATA%`. L'app è un eseguibile GUI
  senza console, quindi prima ogni `qDebug`/`qWarning` finiva nel nulla.
- `SkyDolly/src/CrashHandler_Windows.cpp`: `SetUnhandledExceptionFilter` + minidump + stack trace su
  disco. Prima un access violation terminava il processo senza lasciare traccia.

---

## Problemi aperti

1. **`3rdParty/SimConnect/` è vuota.** Senza i tre file dell'SDK la CI salta il plugin di
   connessione e non produce il pacchetto. Istruzioni in `3rdParty/SimConnect/README.md`.

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

- **SDK MSFS 2024**: in MSFS, Opzioni → Generali → Sviluppatori → Developer Mode ON, poi
  Help → SDK Installer. Si installa in `C:\MSFS 2024 SDK`. Va fatto a mano: non esiste un download
  pubblico diretto (verificato, risponde 404).
- **Visual Studio Build Tools 2022** con workload C++, se non già presenti.
- **Tutte le prove dentro il simulatore**: registrazione, replay, motori, suoni, pannello.

---

## Ripartire da una macchina nuova

```
git clone --recurse-submodules -b msfs2024 https://github.com/EdoWhite/SkyDolly24.git
```

Il `--recurse-submodules` è necessario: senza `3rdParty/{cpptrace,geographiclib,ordered-map}`
CMake non configura.

Poi, in una sessione di Claude Code aperta nella cartella del progetto, è sufficiente chiedere di
leggere questo file e `Plan.md` e proseguire dalla prima fase non completata.

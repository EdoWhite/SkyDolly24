# Sky Dolly — note per Claude Code

Fork di [till213/SkyDolly](https://github.com/till213/SkyDolly) v0.20.0, in corso di adattamento a
**Microsoft Flight Simulator 2024**. Applicazione Qt 6 / C++20, si collega al simulatore via
SimConnect. **Solo Windows**: macOS e Linux compilano ma non sono un obiettivo.

## Lavoro in corso

Il porting a MSFS 2024 avviene sul branch `msfs2024`. Prima di intervenire, leggere:

- [doc/MSFS2024/Status.md](doc/MSFS2024/Status.md) — a che punto siamo, problemi aperti, cosa serve
  dall'utente. **Aggiornare questo file alla fine di ogni fase.**
- [doc/MSFS2024/Plan.md](doc/MSFS2024/Plan.md) — il piano completo, fase per fase, con le cause
  individuate dietro ogni problema.

## Build

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSKY_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Il plugin di connessione richiede l'SDK di MSFS 2024 (`C:\MSFS 2024 SDK`, oppure la variabile
d'ambiente `MSFS2024_SDK`, oppure una copia in `3rdParty/SimConnect/`). Senza, CMake lo salta con un
avviso e l'applicazione non può parlare col simulatore. Opzioni in `BUILD.md`; la logica di ricerca
è in `cmake/FindSimConnect.cmake`.

La workflow `.github/workflows/windows-release.yml` compila, testa e produce il pacchetto come
artifact scaricabile.

## Struttura

Architettura modulare a plugin, descritta in [doc/Design/Architecture.md](doc/Design/Architecture.md).

- `src/Kernel` — fondamenta (impostazioni, matematica, versione, log)
- `src/Model` — dati di volo e interpolazione dei campioni
- `src/Persistence` — logbook SQLite; le migrazioni sono marcatori `@migr` in coda a
  `src/Persistence/src/Dao/SQLite/migr/LogbookMigration.sql`, non file separati
- `src/PluginManager` — macchina a stati di registrazione e replay (`AbstractSkyConnect`) e
  caricamento dei plugin
- `src/Plugins/Connect/MSFSSimConnectPlugin` — tutto il codice SimConnect
- `src/Plugins/{Module,Flight,Location}` — moduli dell'interfaccia e import/export

## Convenzioni

- I sotto-record delle variabili di simulazione seguono lo schema documentato in
  `src/Plugins/Connect/MSFSSimConnectPlugin/doc/SimVars.md` (`*Common`, `*Core`, `*Event`, `*All`,
  `*User`, `*Ai`).
- Mai chiamare `SimConnect_Open`/`SimConnect_Close` dall'interno della callback di dispatch:
  rimandare all'event loop con `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
- Commenti e messaggi di commit in inglese, come il resto del progetto.

# Sky Dolly add-on for Microsoft Flight Simulator 2024

`skydolly-panel/` is a Community package that adds a Sky Dolly button to the simulator's toolbar.
Opening it gives transport controls, a scrubbable timeline, replay speed and the recent flights from
the logbook, without leaving the simulator.

The panel does not record or replay anything itself: it is a remote control. All the work stays in
the Sky Dolly process, which the panel reaches over HTTP on the loopback interface
(`http://127.0.0.1:17285` by default). Sky Dolly must be running for the panel to do anything; when
it is not, the panel says so and keeps retrying.

## Installing

Copy `skydolly-panel/` into the MSFS 2024 `Community` folder:

- Steam: `%APPDATA%\Microsoft Flight Simulator 2024\Packages\Community`
- MS Store: `%LOCALAPPDATA%\Packages\Microsoft.Limitless_8wekyb3d8bbwe\LocalCache\Packages\Community`

The exact location is whatever `InstalledPackagesPath` in `UserCfg.opt` points at, which the user may
have moved.

After copying, regenerate `layout.json` if any file changed: it lists every file with its size, and
the simulator refuses to load a package whose layout does not match what is on disk.

## The same interface in a browser

The Sky Dolly service serves this very panel at `http://127.0.0.1:17285/`. That is not a
consolation prize: it is the same HTML, CSS and JavaScript, embedded into the application as a Qt
resource from these files, so there is one source of truth and no chance of the two drifting apart.

It exists for two reasons. It is how the panel can be developed and checked without launching the
simulator. And it is the fallback: custom toolbar panels are a community technique rather than a
documented SDK feature, so a Sim Update can break the in-game route. The browser route depends on
nothing but a TCP socket.

## What registers the toolbar icon

`InGamePanels/InGamePanel_SkyDolly.spb`, and nothing else. It is a compiled binary; the source it
is built from, and the reason it cannot be written by hand, are in
[`../panel-project/README.md`](../panel-project/README.md).

The first in-sim test was run without it - an XML descriptor sat in `html_ui/` instead, on the
assumption that the simulator scanned that folder. It does not, and no shipped panel has such a
file. Comparing this package against `fcr-embedded` and the twenty `fs-base-ingamepanels-*` packages
inside MSFS 2024 shows the layout they all share, which is now the layout here:

```
InGamePanels/InGamePanel_<Name>.spb          <- the registration
html_ui/InGamePanels/<Name>/<Name>.html      <- the panel itself
html_ui/icons/toolbar/ICON_TOOLBAR_<X>.svg   <- the icon
```

If the button still does not appear after installing:

1. check that `layout.json` matches the files on disk, sizes included - the `.spb` has to be listed
   there too;
2. check the Coherent debugger at `http://127.0.0.1:19999` with Developer Mode on, which lists the
   panels the simulator actually loaded;
3. use `http://127.0.0.1:17285/` meanwhile - everything works there.

Nothing about the Sky Dolly side depends on the answer: the service, the API and the panel markup
are the same either way.

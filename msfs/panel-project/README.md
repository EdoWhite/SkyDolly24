# The toolbar panel descriptor

The icon in the simulator's toolbar comes from **one binary file**:
`skydolly-panel/InGamePanels/InGamePanel_SkyDolly.spb`. Without it the package still installs, the
HTML is still there, and nothing appears in the toolbar. This directory builds that file.

## Why it cannot simply be written by hand

The `.spb` is a SimPropBinary: the compiled form of the XML in `PackageSources/`. Its strings are
obfuscated, and the element names are not present as plain text inside `FlightSimulator2024.exe`
either, so the loader is comparing shapes that are already encoded. There is no way to produce one
except by compiling it.

The schema is **not documented**. Asobo has said so on the developer forum, and the SDK ships no
sample for it. What is in `PackageSources/SkyDollyPanel.xml` is the shape the working third-party
panels use, and it agrees with what ships inside MSFS 2024 itself - compare `fcr-embedded` and the
`fs-base-ingamepanels-*` packages in the simulator's own `Packages` folder, which have exactly the
layout this add-on has:

```
InGamePanels/InGamePanel_<Name>.spb          <- the registration
html_ui/InGamePanels/<Name>/<Name>.html      <- the panel itself
html_ui/icons/toolbar/ICON_TOOLBAR_<X>.svg   <- the icon
```

Note that the icon path is the 2024 one. MSFS 2020 kept toolbar icons under
`html_ui/Textures/Menu/toolbar/`, so a 2020 template will point at the wrong place.

## Building it

Needs MSFS 2024 installed - `fspackagetool` builds by **launching the simulator** in a build mode,
so do not run this while flying.

```
"C:\MSFS 2024 SDK\Tools\bin\fspackagetool.exe" SkyDollyPanel.xml -nopause
```

The output lands in `Packages\skydolly-panel\InGamePanels\`. Copy the `.spb` next to the rest of the
add-on and record it in the layout:

```
copy Packages\skydolly-panel\InGamePanels\InGamePanel_SkyDolly.spb ..\skydolly-panel\InGamePanels\
```

`skydolly-panel/layout.json` then needs an entry for it, with the file's real size - the simulator
checks that, and a wrong size makes it ignore the file.

**The built `.spb` is committed** to `skydolly-panel/InGamePanels/`. Users installing the add-on do
not have the SDK, and this file changes only when the panel is renamed, moved or resized.

## If the icon still does not appear

Turn on Developer Mode in the simulator and open the Coherent debugger at
<http://127.0.0.1:19999>: it lists the panels that were actually loaded. Check also that every path
in `layout.json` matches a file on disk, sizes included.

Whatever happens here, the same interface is served at <http://127.0.0.1:17285/> from the same
files, and no Sim Update can reach that.

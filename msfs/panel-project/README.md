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

It prints almost nothing; `_Temp\_RPTErrors.xml` is where it says whether anything went wrong. The
output lands in `Packages\skydolly-panel\InGamePanels\SkyDollyPanel.spb` - note the name comes from
the name of the source file in `PackageSources\`, not from the `Filename` element inside it.

Copy it next to the rest of the add-on, then regenerate the layout from what is on disk, because
the simulator checks the size of every file it lists and ignores one that does not match:

```powershell
$pkg = '..\skydolly-panel'
Copy-Item Packages\skydolly-panel\InGamePanels\SkyDollyPanel.spb "$pkg\InGamePanels\" -Force
$entries = Get-ChildItem $pkg -Recurse -File |
    Where-Object { $_.Name -notin @('layout.json','manifest.json') } | ForEach-Object {
        [pscustomobject]@{
            path = $_.FullName.Replace("$((Resolve-Path $pkg).Path)\",'').Replace('\','/')
            size = $_.Length
            date = $_.LastWriteTimeUtc.ToFileTimeUtc()
        }
    }
[pscustomobject]@{ content = @($entries) } | ConvertTo-Json -Depth 4 |
    Set-Content "$pkg\layout.json" -Encoding utf8
```

**The built `.spb` is committed** to `skydolly-panel/InGamePanels/`. Users installing the add-on do
not have the SDK, and this file changes only when the panel is renamed, moved or resized.

## Checking the output without the simulator

The strings inside are obfuscated but the structure is not: it is a flat list of
`<uint32 property><uint32 length><bytes>` records, with string lengths including their terminator
and numbers stored as little-endian floats. That is enough to confirm a build did what was asked
without starting MSFS - the descriptor here decompiles to a 14+1 byte id, a 53+1 byte url, a 21+1
byte icon name, and the floats 18, 30, 22, 55, 20, 2.

## If the icon still does not appear

Turn on Developer Mode in the simulator and open the Coherent debugger at
<http://127.0.0.1:19999>: it lists the panels that were actually loaded. Check also that every path
in `layout.json` matches a file on disk, sizes included.

Whatever happens here, the same interface is served at <http://127.0.0.1:17285/> from the same
files, and no Sim Update can reach that.

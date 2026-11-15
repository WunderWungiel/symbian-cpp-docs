# Examples

Small projects that show the idioms from the docs, laid out the standard way (`group/`, `inc/`, `src/`, `data/`, `gfx/`, `sis/`). They target Symbian OS 9.x (S60 3rd Edition and later).

| Project | Shows |
|---------|-------|
| [HelloConsole](HelloConsole) | `E32Main`, cleanup stack, `TRAP`, heap checks, active scheduler, a timer active object |
| [HelloGui](HelloGui) | S60 GUI architecture (application/document/AppUi/container), resources, menu, notes, registration file, SVG→MIF icon, layout changes |

## Building (SDK command line)

```bat
cd HelloConsole\group
bldmake bldfiles
abld build winscw udeb      :: emulator
abld build gcce urel        :: device
cd ..\sis
makesis HelloConsole.pkg
signsis -s HelloConsole.sis HelloConsole.sisx my.cer my.key password
```

The PKG files use Carbide.c++ path variables (`$(EPOCROOT)`, `$(PLATFORM)`, `$(TARGET)`). For plain `makesis`, replace them with real paths such as `\Symbian\9.2\S60_3rd_FP1\Epoc32\release\gcce\urel\...`. You can also import `group\bld.inf` into Carbide.c++ (*File → Import → Symbian OS Bld.inf file*).

The UIDs are in the test range (`0xE…`). Change them before you distribute anything.

These examples were **not compiled** when this repository was written, because no Symbian SDK was available in that environment. They follow the documented SDK APIs. Please report anything that fails to build.

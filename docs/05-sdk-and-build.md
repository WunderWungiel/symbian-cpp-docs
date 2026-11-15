# 5. SDK, Build Tools, Emulator and Packaging

> **Scope:** choosing and installing an SDK, the command-line build (`bldmake`/`abld`), compilers and targets, freezing DLL exports, the emulator, PKG files, `makesis`, `makekeys`, `signsis`, and common installation errors.

## 5.1 Choosing an SDK

Before Symbian^1 there was no single "Symbian 9.x SDK"; you used the SDK of a **UI platform** (S60 or UIQ). For S60 the SDKs were distributed by Forum Nokia, one per edition/Feature Pack:

| SDK | Target devices (examples) |
|-----|---------------------------|
| S60 3rd Edition (Maintenance Release) — Symbian 9.1 | Nokia 3250, N73, E61, N91 |
| S60 3rd Edition FP1 — 9.2 | Nokia E90, N95, E71, 6120c |
| S60 3rd Edition FP2 — 9.3 | Nokia N96, E52, 5730, 6220c |
| S60 5th Edition — 9.4 | Nokia 5800, N97, 5530, X6 |
| Symbian^3 SDK 1.0 / Nokia Symbian SDKs (Qt SDK) | Nokia N8, C7, E7, 808 |

All Feature Packs within an edition are backward compatible, and 5th Edition runs 3rd Edition binaries. **Build with the oldest SDK you want to support** (typically S60 3rd Edition MR or FP1). Beware subtle differences: for instance, the binary-encoded SVG app-icon format changed in 3rd Ed. FP1, so an app built with an FP1/FP2 SDK may show no icon on an original 3rd Edition phone. While learning, use the SDK that matches your test device.

The SDK only contains **public APIs** with compatibility guarantees. Other ("private") APIs were available through Forum Nokia **SDK API Extension plug-ins**, Symbian partner kits (SODK, BAK, DevKit), or partner programs. Today, leaked/unofficial headers circulate in the hobbyist community — use them with care; they carry no compatibility guarantees.

> **Today (archival):** Nokia's download pages are gone. SDKs survive in archives (e.g. the Internet Archive and community mirrors). See [Modern toolchains](06-ide-and-toolchains.md) for current options.

## 5.2 Installing an SDK (classic Windows setup)

* Requirements of the era: Windows XP/Vista (32-bit), **ActivePerl 5.6.1** (exactly 5.6.1.x — newer versions break the build scripts), and a Java Runtime (for SISAR, AIF builder, emulator preferences).
* Run the installer as Administrator.
* Install to the default `C:\Symbian\9.x\S60_3rd_xx\`. The path must have **no spaces**, and **projects must be on the same drive as the SDK** (the build uses `EPOCROOT`-relative paths).
* Many SDKs predate Vista/7; known workarounds (manifest tweaks, running in compatibility mode) were collected on the Forum Nokia wiki ("Moving to Windows Vista").

### SDK layout

```
C:\Symbian\9.2\S60_3rd_FP1\
├── Epoc32\
│   ├── BUILD\       generated makefiles and intermediates
│   ├── data\        emulator configuration (epoc.ini) and data; data\z\ = emulated ROM data
│   ├── gcc\         build helpers (bison, flex…)
│   ├── include\     ALL public headers (.h, .rh, .hrh) and generated .rsg/.mbg
│   ├── kit\         documentation plug-ins for IDEs
│   ├── release\     libraries (.lib, .dso) and binaries per target/variant
│   │   ├── armv5\urel\   import libs for device builds
│   │   ├── gcce\urel\
│   │   └── winscw\udeb\  emulator binaries; …\z\ = emulator Z: drive
│   ├── tools\       command-line tools (bldmake, abld, makesis, signsis, rcomp, bmconv, mifconv, elf2e32…)
│   └── winscw\      emulator drives: c\, d\, e\…
├── Examples\        Symbian OS examples (mostly console)
├── S60Doc\          documentation (_index.chm = SDK Help)
├── S60Ex\           S60 platform examples
└── S60Tools\        Ecmt (emulator/device connectivity), MBM viewer, SimPSY configurator, svg2svgt
```

### Selecting the active SDK: `devices`

With several SDKs installed, the active one is chosen via `EPOCROOT`/`PATH`, managed by `devices.exe` (it edits `C:\Program Files\Common Files\Symbian\devices.xml`):

```bat
C:\> devices
S60_5th_Edition_SDK_v1.0:com.nokia.s60
S60_3rd_FP1:com.nokia.s60 - default
S60_3rd_MR:com.nokia.s60

C:\> devices -default
C:\> devices -setdefault @S60_3rd_MR:com.nokia.s60
C:\> devices -info @S60_3rd_MR:com.nokia.s60
```

`devices.xml` entries look like:

```xml
<device id="S60_3rd_FP2" name="com.nokia.s60" default="no" userdeletable="yes">
  <epocroot>C:\Symbian\9.3\S60_3rd_FP2</epocroot>
  <toolsroot>C:\Symbian\9.3\S60_3rd_FP2</toolsroot>
</device>
```

`EPOCROOT` must end with a backslash and normally omits the drive letter (e.g. `\Symbian\9.2\S60_3rd_FP1\`).

## 5.3 Compilers, build targets and variants

| Target | Compiler | Notes |
|--------|----------|-------|
| `WINSCW` | Nokia x86 (ex-Metrowerks CodeWarrior), `mwccsym2.exe` | Emulator builds (x86). Older SDKs used MSVC with target `WINS`. |
| `GCCE` | **GCCE** (GCC for embedded, CodeSourcery, v3.4.3 in the SDKs; 4.x used by some at their own risk) | Free device builds, ARMv5, EABI v2. **The usual choice.** |
| `ARMV5` | ARM **RVCT 2.2** (commercial, part of RVDS) | Better optimization; EABI v2. Building `ARMV5` without RVCT installed is a common beginner error. |
| `ARMV5_ABIV1` | RVCT | Legacy EABI v1 (runtime- but not link-compatible with v2) |
| `ARMV6` | RVCT | ARMv6 instruction set |

* **EABI** (Embedded Application Binary Interface) is the ARM industry standard for calling conventions and binary layout; since GCCE and RVCT both produce **EABI v2**, their libraries interoperate.
* ARMv5 supports two instruction sets: 32-bit **ARM** and 16-bit **Thumb** (smaller, lower power, slower). Symbian user code defaults to Thumb for RVCT; use `build_as_arm` / `ALWAYS_BUILD_AS_ARM` for speed-critical code.
* Variants: **UDEB** (Unicode debug — symbols, extra checks; many APIs **panic** on misuse where UREL returns an error) and **UREL** (Unicode release). Ship UREL.

## 5.4 Building from the command line

```bat
cd MyApp\group
bldmake bldfiles               :: bld.inf -> abld.bat (+ first-level makefiles)
abld build winscw udeb          :: emulator debug build
abld build gcce urel            :: device release build
```

`bldmake bldfiles` is needed only when `abld.bat` is missing or `bld.inf` changed. `abld build` = the sequence:

| Step | Does |
|------|------|
| `abld export` | Copy `PRJ_EXPORTS` files |
| `abld makefile` | `makmake` generates per-MMP, per-platform makefiles in `\epoc32\build\<path>\<mmp>\<platform>\` |
| `abld library` | Create import libraries from `.def` files of DLLs |
| `abld resource` | Preprocess (`cpp`) + compile `.rss` with **`rcomp`** → `.rsc` + `.rsg`; build `.mbm` with **`bmconv`** → `.mbm` + `.mbg` |
| `abld target` | Compile `.cpp` → `.o`; link to ELF; convert ELF to Symbian **E32Image** format with **`elf2e32`** (EABI v2) or `elftran` (v1); output to `\epoc32\release\<target>\<variant>\` |
| `abld final` | Run extension makefiles (`.mk`), e.g. `mifconv` for SVG icons |

Each step can be run on its own (`abld resource gcce urel`). Other useful commands:

```bat
abld build gcce urel MyApp      :: build one component (MMP name without extension)
abld -v build gcce urel         :: verbose: show actual compiler command lines
abld -k build ...               :: keep going after errors
abld freeze gcce                :: freeze DLL exports into .def files
abld clean                      :: delete what "target" produced
abld cleanexport                :: delete exported files
abld reallyclean                :: clean + cleanexport + makefiles  (use this one)
bldmake clean                   :: delete abld.bat and first-level makefiles (after editing bld.inf)
```

**When builds behave oddly** (changes not showing, inexplicable errors): `abld reallyclean`, `bldmake clean`, `bldmake bldfiles`, rebuild. If that fails, delete `\epoc32\build\<your path>` manually.

## 5.5 Freezing DLL exports (`.def` files)

Symbian DLLs export functions **by ordinal only** — no names. Re-ordering exports silently breaks every client binary. A **module definition file** pins the ordinals:

```text
EXPORTS
    _ZN4CLib4NewLEv  @ 1 NONAME   ; CLib::NewL()
    _ZN4CLib5NewLCEv @ 2 NONAME   ; CLib::NewLC()
    _ZN4CLibD0Ev     @ 3 NONAME   ; CLib::~CLib()
```

* Don't write `.def` files by hand: build, then run **`abld freeze`**, then rebuild (`abld library` regenerates the import library).
* They live in `bwins\` (WINSCW, mangled MSVC-style names like `?NewL@CLib@@SAPAV1@XZ`) and `eabi\` (GCCE/ARMV5, Itanium-ABI mangled names) next to `group\`.
* Only **append** new exports; never remove or reorder an existing one in a released DLL (mark removed ones `ABSENT`).
* An unfrozen DLL produces a warning ("Frozen .def file not found") and an unusable or regenerated import lib; `EXPORTUNFROZEN` in the MMP allows linking against unfrozen exports during development.

See [DLLs and libraries](13-dlls-libraries.md).

## 5.6 The emulator

Start it with `epoc` (or `sdk` / Start menu); WINSCW-built EXEs appear in it immediately — no packaging needed.

### Hot keys (S60 3rd/5th Edition emulator)

| Key | Action |
|-----|--------|
| `Esc` | Cancel / close dialog |
| `F1` | Options menu |
| `F9` / `F10` | Screen off–on / simulate battery removal |
| `~` (or numpad Enter) | Selection key |
| `Alt+1`, `Alt+2` | Left / right softkey |
| Arrows | Navigation keys |
| `0`–`9`, `*`, `#` | ITU-T keypad |
| `Home` | Applications (menu) key |
| `Ctrl+H`, `Backspace` | Clear key |
| `Shift` | Edit (pencil) key |
| `Ctrl+Alt+Shift+O` | Rotate emulator skin 180° |
| `Ctrl+Alt+Shift+J` | Draw coloured borders around controls |
| `Ctrl+Alt+Shift+P` | **Resource failure tool** — simulate heap/WServ/file-server allocation failures (random or every N-th) |
| `Ctrl+Alt+Shift+Q` | Turn heap-failure simulation off |
| `Ctrl+Alt+Shift+R` | Redraw the whole screen |
| `Ctrl+Alt+Shift+F` / `G` | Window Server auto-flush on/off (reveals flicker) |
| `Ctrl+Alt+Shift+M` | "Move Me" window to test partial redraws |
| `Ctrl+Alt+Shift+Z` | Simulate fast key input (A–J) |

### Menus and settings
* **File → Open / Open URL** – open a file or URL as the phone would (test MIME associations, browser).
* **Tools → Utilities** – simulate MMC insert/remove, charger, SIM, accessories, incoming SMS/MMS, alarms, **location and routes**.
* **Tools → Diagnostics** – memory/CPU per process, HTTP log.
* **Tools → Preferences** (writes `\epoc32\data\epoc.ini`):
  * *C++ Debug*: log to debugger; **Just-In-Time** (off = show the panic dialog instead of killing the emulator); **Enable EPOCWIND.OUT logging** (written to `%TEMP%\EPOCWIND.OUT` — a key log for panics and PlatSec diagnostics); extended panic code file (`ErrRd`, see [Errors](08-errors-leaves-cleanup.md)).
  * *General*: RAM size, initial resolution, Bluetooth/IrDA COM ports, network proxy, minimal ("partial") vs. full server start-up (FP2+).
  * *Platform Security*: debug messages, **panic/leave on insecure API calls**, disable checks for chosen capabilities.
* Useful `epoc.ini` keys: `PlatSecEnforcement ON`, `PlatSecDiagnostics ON`, `JustInTime none`, `LogToFile 1`.

### Emulator file system
* `C:` → `\epoc32\winscw\c\`, `D:` → `\epoc32\winscw\d\`, etc.
* `Z:` → `\epoc32\release\winscw\udeb\z\` (and `\epoc32\data\z\` for resources).
* `\sys\bin\` is redirected to `\epoc32\release\winscw\udeb\` — that's why built EXEs appear without installation.

### Emulator vs. real device — always test on hardware
* The emulator is **one Windows process**; Symbian "processes" are threads inside it. Process-level behaviour (memory isolation, process-kill, scheduling, priorities) differs.
* The installer isn't used, so **capability checks are lax by default** (the app gets everything it declares) and **private directories aren't created automatically**.
* Stacks grow automatically in the emulator; on a device, exceeding `EPOCSTACKSIZE` crashes.
* Device ARM CPUs require **aligned memory access** (4-byte for 32-bit values); misaligned reads that work on x86 fault on ARM. `sizeof` of structs is a multiple of 4.
* No hardware floating point on many phones; emulator performance is unrepresentative.
* Writable static data in DLLs behaves differently (see [DLLs](13-dlls-libraries.md)).

## 5.7 Packaging: the PKG file

A SIS package is built in four steps: build for `GCCE`/`ARMV5` → write a `.pkg` → `makesis` → `signsis`.

### Complete example

```text
; MyApp.pkg — UTF-8 encoded because of the IU option
&EN, RU                                           ; package languages

#{"My App", "Моя программа"}, (0xE1234567), 1, 2, 0, TYPE=SA, IU

%{"Example Ltd", "Пример"}                        ; localized vendor
:"Example Ltd"                                    ; unique (non-localized) vendor

; Supported platform: S60 3rd Edition and later
[0x101F7961], 0, 0, 0, {"S60ProductID", "S60ProductID"}

; Dependency on another package (Open C / PIPS)
(0x20009A80), 1, 0, 0, {"Symbian OS PIPS", "Symbian OS PIPS"}

; License text shown during installation (not installed)
{"..\docs\license_en.txt", "..\docs\license_ru.txt"} - "", FT, TA

; Binaries
"$(EPOCROOT)Epoc32\release\$(PLATFORM)\$(TARGET)\MyApp_0xE1234567.exe" - "!:\sys\bin\MyApp_0xE1234567.exe"

; Resources: install only the language matching the phone
{
"$(EPOCROOT)Epoc32\data\z\resource\apps\MyApp_0xE1234567.r01"
"$(EPOCROOT)Epoc32\data\z\resource\apps\MyApp_0xE1234567.r16"
} - "!:\resource\apps\MyApp_0xE1234567.rsc"

"$(EPOCROOT)Epoc32\data\z\resource\apps\MyApp_0xE1234567_aif.mif" - "!:\resource\apps\MyApp_0xE1234567_aif.mif"
"$(EPOCROOT)Epoc32\data\z\private\10003a3f\apps\MyApp_0xE1234567_reg.rsc" - "!:\private\10003a3f\import\apps\MyApp_0xE1234567_reg.rsc"

; Private data
"..\data\defaults.ini" - "!:\private\E1234567\defaults.ini"
```

### Syntax reference

| Line | Syntax | Notes |
|------|--------|-------|
| Languages | `&EN, RU, FR(dialect)` | Default `&EN`. Affects package name, vendor, file choice, options. Not the same as app localization. |
| Header | `#{"Name lang1", ...}, (PID), major, minor, build[, options]` | **PID** (package UID): conventionally the main app's UID3 (required for e.g. Startup List). |
| Header options | `SH`/`SHUTDOWNAPPS` (close all apps first), `NC`/`NOCOMPRESS`, `IU`/`IUNICODE` (file is UTF-8; **required for non-Latin text**), `TYPE=` | |
| Package types | `SA`/`SISAPP` (default; replaces an installed package with same PID), `SP`/`SISPATCH` (add-on: same PID, different name, may only **add** files, separately removable), `PU`/`PARTIALUPGRADE` (same PID and name; add/replace files; not separately removable), `PA`/`PIAPP` (pre-installed app, registration only), `PP`/`PIPATCH` | |
| Vendor | `%{"Vendor lang1", ...}` and `:"Unique vendor"` | Self-signed packages may show "Unknown" anyway. |
| Platform | `[ProductUID], major, minor, build, {"name"...}` | Version `0,0,0` = any; ranges `1,0,0 ~ 2,*,*`; `*` or `-1` wildcard. Missing → "not compatible" warning on every install. |
| Dependency | `(PID), version-range, {"name"...}` | Missing dependency → warning dialog. |
| File | `"source" - "dest"[, options]` | `!:` = user-chosen drive (prompt shown). Empty dest = shipped but not installed. Source may use `$(EPOCROOT)`, `$(PLATFORM)`, `$(TARGET)` (Carbide substitutes these). |
| File options | `FF` (plain file, default), `FT` + `TC`/`TA` (show text, Continue / Yes-No abort), `FR` + `RI`/`RR`/`RB` [+`RW`] (run on install/remove/both, optionally wait), `FM`, `"mime/type"` (open with handler) | `FR` may be ignored for self-signed packages. |
| Language-dependent file | `{"a.r01" "a.r16"} - "dest"` | One source per package language, same order. |
| Options | `!({"Opt1 lang1", "Opt2 lang1"}, {"Opt1 lang2", ...})` | Then `IF option1 … ENDIF`. |
| Conditions | `IF expr … ELSEIF … ELSE … ENDIF` | Attributes like `MACHINEUID = 0x2000DA56` (5800); functions `package(PID)`, `exists("path")` (not on drive roots), `appprop(PID, prop)`; `NOT`, `AND`, `OR`. |
| Properties | `+(0=3, 1=2, 2=0)` | Numeric properties readable by later packages via `appprop()`. |
| Embedded SIS | `@"other.sis", (PID)` | Installed as a separate step; failure aborts the whole install. Uninstalled with the parent unless something else depends on it. |
| Comment | `; text` | |

### Platform Product UIDs

| Platform | Product UID |
|----------|-------------|
| S60 3rd Edition (9.1) | `0x101F7961` |
| S60 3rd Edition FP1 (9.2) | `0x102032BE` |
| S60 3rd Edition FP2 (9.3) | `0x102752AE` |
| S60 5th Edition (9.4) | `0x1028315F` |
| Symbian^3 | `0x20022E6D` |
| UIQ 3.0 (9.1) | `0x101F6300` |
| UIQ 3.1 (9.2) | `0x101F63DF` |

Device-model UIDs also work (e.g. Nokia N95 `0x2000060B`, Nokia 5800 `0x2000DA56`). Since 9.x is upward compatible, declaring the oldest platform you support (S60 3rd = `0x101F7961`) is enough. Detect the platform at install time with e.g. `IF exists("z:\system\install\Series60v3.0.sis")`.

**Uninstall hygiene:** the installer removes only files it installed. Files your app creates are removed only if they're in `\private\<SID>\` — keep all app-generated data there (also a Symbian Signed test criterion).

## 5.8 `makesis`, `makekeys`, `signsis`

```bat
makesis [-h] [-i] [-s] [-v] [-d dir] MyApp.pkg [MyApp.sis]
::  -h PKG syntax help   -s create ROM stub SIS   -v verbose   -d search dir

makekeys -cert [-v] [-password pw] [-len 512..4096] -dname "CN=Name OR=Org CO=RU EM=me@example.com" my.key my.cer
::  creates a self-signed certificate valid for one year; dname needs at least two fields:
::  CN name, OU unit, OR organisation, LO locality, ST state, CO 2-letter country, EM e-mail
makekeys -view my.cer

signsis -s MyApp.sis MyApp.sisx my.cer my.key [password]
signsis -o MyApp.sisx          :: show signatures
signsis -u MyApp.sisx out.sis  :: remove the last signature
::  -cd / -cr choose DSA / RSA
```

`.sisx` conventionally denotes a signed package; many distribute it renamed back to `.sis`. `createsis` (in later SDKs) wraps makesis + makekeys + signsis in one step.

## 5.9 Troubleshooting installation errors

Nearly all install failures are Platform Security issues:

| Symptom | Likely cause |
|---------|-------------|
| "Unable to install" / "File corrupted" | File destined for a forbidden location (another app's `\private\`, `Z:`); malformed PKG |
| "Update error" / "Cannot install" | File already installed by another package (EXE/DLL name clash — see naming rules) |
| "Unable to install" | Executable with SID 0 or a SID already used by another installed executable |
| "Required application access not granted" | Declared capabilities not covered by the certificate(s) |
| "Certificate error" / "Expired certificate" | Certificate expired or not yet valid — check the phone's date (Open Signed Offline DevCerts sometimes only become valid the next day due to time zones). Also: phone's *App. manager → Settings → Online certificate check* should be off for self-signed. |
| "Certificate restricted" | DevCert doesn't list this device's IMEI |
| "Unable to install a protected application from an untrusted supplier" | Protected-range UID3/SID (`0x2…`) in a self-signed package; use `0xE…`/`0xA…` |
| "Software installation allowed only from trusted suppliers" | *App. manager → Settings → Software installation* must be set to **All** for self-signed packages |

The Symbian developer wiki once maintained "Troubleshooting Installation Errors" with a full list of messages.

## See also
* [Project structure](03-project-structure.md)
* [IDEs and modern toolchains](06-ide-and-toolchains.md)
* [Signing and certification](23-signing-certification.md)

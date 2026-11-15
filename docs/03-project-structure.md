# 3. Project Structure: Folders, `bld.inf` and MMP Files

> **Scope:** how a Symbian C++ project is laid out and described to the build system.

## 3.1 Standard folder layout

```
MyApp/
├── data/     resource files: *.rss, *.rssi, *.rls / *.loc / *.lXX, *.rh (own)
├── inc/      headers: *.h, *.hrh (shared C++/resource constants), *.pan (panic codes)
├── src/      sources: *.cpp
├── gfx/      images: *.bmp, *.svg
├── group/    build files: bld.inf, *.mmp, *.mk (extension makefiles)
└── sis/      packaging: *.pkg (→ .sis / .sisx)
```

Carbide.c++ wizards create exactly this layout; keep it — tools, examples and other developers expect it.

## 3.2 `bld.inf` — the component list

One `bld.inf` per project, in `group/`. `bldmake bldfiles` turns it into `abld.bat`. Plain text, sections in any order, each optional and repeatable:

```text
PRJ_PLATFORMS
DEFAULT

PRJ_MMPFILES
gnumakefile icons_scalable_dc.mk   // build icons first
helloworld.mmp

PRJ_EXPORTS
..\inc\myapi.h                      // -> \epoc32\include\myapi.h
mydata.dat  e:\data\appdata.dat     // drive letter -> \epoc32\data\e\data\appdata.dat
```

| Section | Purpose |
|---------|---------|
| `PRJ_PLATFORMS` | Target platforms: `WINSCW`, `ARMV5`, `GCCE`… `DEFAULT` = all supported; exclude with `-`, e.g. `DEFAULT -WINSCW`. |
| `PRJ_MMPFILES` | Components, **in build order**: `file.mmp [build_as_arm] [tidy]`, or `makefile`/`nmakefile` (nmake) / `gnumakefile` (GNU make) `file.mk`. `tidy` = intermediate component, deleted after build. `build_as_arm` = ARM (not Thumb) instruction set for ARMV5. |
| `PRJ_TESTMMPFILES` | Like `PRJ_MMPFILES` but for test code (`abld test build`); attributes `manual`, `support`. |
| `PRJ_EXPORTS` | Copy files during `abld export`. Source path relative to `bld.inf`; destination relative to `\epoc32\include\`, or, if it has a drive letter, `\epoc32\data\<drive>\`. `:zip archive.zip [dest]` unpacks a zip. |
| `PRJ_TESTEXPORTS` | Like exports, but destination relative to `bld.inf`; no `:zip`. |

Conditionals work in `bld.inf` and MMP files:

```text
#if defined(WINSCW)
emulator_only.mmp
#endif
```

**Order matters** in multi-component projects: e.g. a client/server project lists the server, then the client DLL, then the app using the client DLL.

## 3.3 MMP files — one per binary

An MMP file describes one EXE, DLL or other target. One statement per line; parameters separated by spaces; order irrelevant; some statements single-use, others repeatable.

Minimal console EXE:

```text
TARGET          helloworld.exe
TARGETTYPE      exe
UID             0 0xE21F6D60
USERINCLUDE     ..\inc
SYSTEMINCLUDE   \epoc32\include
SOURCEPATH      ..\src
SOURCE          helloworld.cpp
LIBRARY         euser.lib
```

Typical S60 3rd Edition GUI application:

```text
TARGET            MyApp_0xE1234567.exe
TARGETTYPE        exe
UID               0x100039CE 0xE1234567
SECUREID          0xE1234567        // optional, defaults to UID3
VENDORID          0
CAPABILITY        NetworkServices   // or NONE
EPOCSTACKSIZE     0x5000
EPOCHEAPSIZE      0x1000 0x100000

SOURCEPATH        ..\src
SOURCE            MyApp.cpp MyAppApplication.cpp MyAppDocument.cpp
SOURCE            MyAppAppUi.cpp MyAppContainer.cpp

SOURCEPATH        ..\data
START RESOURCE    MyApp.rss
HEADER
TARGETPATH        \resource\apps
LANG              SC
END

START RESOURCE    MyApp_reg.rss
TARGETPATH        \private\10003a3f\apps
END

USERINCLUDE       ..\inc
SYSTEMINCLUDE     \epoc32\include

LIBRARY           euser.lib apparc.lib cone.lib eikcore.lib avkon.lib
LIBRARY           commonengine.lib efsrv.lib estor.lib aknnotify.lib
LANG              SC
```

### Statement reference

| Statement | Meaning |
|-----------|---------|
| `TARGET name.ext` | Output file name. |
| `TARGETTYPE type` | `exe`, `dll`, `lib` (static), `plugin` (ECom), `exexp` (EXE with exports), `stddll`/`stdexe` (Open C/P.I.P.S.), `ani`, `fsy`, `ldd`, `pdd`… Sets **UID1** (EXE → `0x1000007A`, DLL → `0x10000079`). UID1 can't be set explicitly. |
| `UID [uid2] [uid3]` | UID2 and UID3; both default to 0. For S60 GUI EXEs UID2 is `0x100039CE`; for ECom plug-ins `0x10009D8D`; for plain EXEs/DLLs 0. |
| `SECUREID sid` | Secure ID; default = UID3. Setting a different value is discouraged. |
| `VENDORID vid` | Vendor ID; default 0. |
| `CAPABILITY list` | Space-separated capability names, `NONE` (default when omitted) or `ALL` with exclusions: `CAPABILITY ALL -TCB -AllFiles -DRM`. |
| `TARGETPATH path` | Pre-9.x and resources: output location under `\epoc32\release\<plat>\<variant>\z\`. On 9.x executables always go to `\sys\bin`. |
| `LANG list` | Two-digit language codes to build resources for (`01` UK English, `03` German, `16` Russian…, see `TLanguage` in `e32const.h`); default `SC` = no localization. |
| `LIBRARY libs` | Import libraries of DLLs to link (`euser.lib efsrv.lib`). Repeatable, any order. |
| `STATICLIBRARY libs` | Static libraries (`.lib` from `TARGETTYPE lib`). |
| `DEBUGLIBRARY libs` | Like `LIBRARY` but only for UDEB builds. |
| `USERINCLUDE dirs` | Search path for `#include "file.h"`. Searched first in the source file's own folder, then `USERINCLUDE`, then `SYSTEMINCLUDE`. |
| `SYSTEMINCLUDE dirs` | Search path for `#include <file.h>`; normally `\epoc32\include` (plus subfolders such as `\epoc32\include\libc` for Open C). |
| `SOURCEPATH dir` | Folder for subsequent `SOURCE`/`RESOURCE`/`BITMAP` statements (relative to the MMP, or absolute from `EPOCROOT`). Can appear several times. |
| `SOURCE files` | `.cpp` files. Don't forget any, and don't list one twice. |
| `START RESOURCE file.rss … END` | Compile a resource file. Sub-statements: `TARGET name` (no extension), `TARGETPATH path` (relative to emulator `z:`), `HEADER` (generate `.rsg`), `LANG codes`, `UID uid2 [uid3]`. Output extension `.rsc` or `.rNN`. Resource UID1 is always `0x101F4A6B`. |
| `START BITMAP file.mbm … END` | Build a multi-bitmap file from BMPs: `TARGETPATH`, `HEADER` (generates `.mbg` with indices), `SOURCEPATH`, `SOURCE c<depth>[,<maskdepth>] a.bmp a_mask.bmp…`. Even positions are masks of the preceding image; all images in one `SOURCE` line share the colour depth. Use mask depth 1 to save memory. SVG icons go into `.mif` files via `mifconv` in an extension makefile. |
| `EPOCHEAPSIZE min max` | Main-thread heap; default 4 KB / 1 MB; rounded to 4 KB pages. Ignored on WINSCW. |
| `EPOCSTACKSIZE bytes` | Main-thread stack; default 8 KB, max 80 KB. **GUI apps need ≥ 20 KB (`0x5000`)** or they crash. Ignored on WINSCW. |
| `EPOCPROCESSPRIORITY p` | `low`, `background`, `foreground` (default), `high`, `windowserver`, `fileserver`, `realtimeserver`, `supervisor`. EXEs only; ignored on WINSCW. |
| `COMPRESSTARGET` / `INFLATECOMPRESSTARGET` / `BYTEPAIRCOMPRESSTARGET` / `NOCOMPRESSTARGET` | Compression of code/data sections; default deflate. Use at most one. Byte-pair is required for demand-paged code. |
| `EPOCALLOWDLLDATA` | Allow writable static data in a DLL (see [DLLs](13-dlls-libraries.md)). |
| `NOSTRICTDEF`, `DEFFILE`, `EXPORTUNFROZEN` | Control of DLL export `.def` files. |
| `MACRO name` | Define a preprocessor macro. |
| `OPTION compiler flags` | Extra compiler options, e.g. `OPTION GCCE -O2`. |
| `ALWAYS_BUILD_AS_ARM` | Compile as ARM rather than Thumb (ARMV5). |

Use `#if defined(WINSCW) … #endif` (or `ARMV5`, `GCCE`, `WINS`) inside MMP files for platform-specific settings, e.g. extra emulator-only libraries.

## 3.4 Naming binaries

Because all binaries share `\sys\bin\`, names must be unique on the device:

* GUI apps: `MyApp.exe` is common, but `MyApp_0xE1234567.exe` is safer.
* DLLs: always include something unique (`mycompany_mylib.dll` or the UID).
* Resource files are referenced from the registration file, so they follow the EXE name: `\resource\apps\MyApp_0xE1234567.rsc`.

See [Applications](12-applications.md) for renaming an app and changing its UIDs consistently.

## 3.5 Other files you'll meet

| File | Meaning |
|------|---------|
| `*.pkg` | Package script for `makesis` (in `sis/`), see [SDK and build](05-sdk-and-build.md). |
| `*.hrh` | Enumerations/constants shared by C++ code **and** resource files (command IDs, control IDs). |
| `*.rsg` | Generated resource IDs (`#define R_MY_DIALOG 0x...`), placed in `\epoc32\include\`. |
| `*.mbg` | Generated bitmap index enum for an `.mbm`/`.mif`. |
| `*.pan` | Convention: panic codes enum + `Panic()` helper. |
| `*.def` | Frozen export ordinals of a DLL (`bwins\`, `eabi\` folders). |
| `*.uidesign` | Carbide.c++ UI Designer state. |
| `*.mk` | Extension makefile, typically converts SVG/BMP icons to `.mif` with `mifconv`. |

## See also
* [Resource files and localization](04-resource-files.md)
* [SDK, build tools, emulator, packaging](05-sdk-and-build.md)

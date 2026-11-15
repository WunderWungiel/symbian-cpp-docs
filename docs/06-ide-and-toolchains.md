# 6. IDEs, Debugging and Toolchains (Then and Now)

> **Scope:** the classic IDEs (Carbide.c++, CodeWarrior, Carbide.vs), on-device debugging with TRK, and what developers use today now that Nokia's infrastructure is gone.
> **Status note:** everything in this chapter that involves Nokia/Symbian web services refers to the 2008–2013 era. Downloads now survive only in archives and community mirrors — verify each link before relying on it.

## 6.1 Classic IDEs

| IDE | Notes |
|-----|-------|
| **Metrowerks CodeWarrior for Symbian OS** | Popular early IDE (its x86 compiler lives on as the WINSCW compiler). Discontinued. |
| Borland C++BuilderX Mobile Studio, Wirelexsoft VistaMax | Obsolete. |
| **Carbide.vs** | Visual Studio add-in; no UI Designer, no TRK. Dropped in 2008. |
| **Carbide.c++** (Eclipse/CDT based) | **The** Symbian IDE. 1.x had paid Developer/Professional/OEM editions; **2.0 (Dec 2008) became free**; later versions (2.x–3.2) added Qt support. Handed to the Symbian Foundation in 2009. Works with S60 3rd/5th Ed., Symbian^3 and UIQ SDKs (1.3 was the last to support pre-9 SDKs). |

### Carbide.c++ essentials
* Install ActivePerl 5.6.1 and at least one SDK first; Carbide finds SDKs via `devices.xml`.
* **New → Symbian OS C++ Project** templates: *Basic console application*, *S60 GUI application* (with/without UI Designer), *Basic static library*, *Basic dynamically linked library*, *Empty project*. Creates the standard layout (`group/`, `inc/`, `src/`, `data/`, `gfx/`, `sis/`).
* **File → Import → Symbian OS Bld.inf file** imports existing command-line projects (choose SDK + build configurations + components).
* **Build configurations** = SDK × target × variant, e.g. *Emulator Debug (WINSCW) [S60_3rd_FP1]*, *Phone Debug (GCCE)*, *Phone Release (GCCE)*, *Phone Release (ARMV5)*.
* **MMP / bld.inf editors** (form-based), **UI Designer** (`.uidesign`, generates Avkon resource/code), **Symbian OS C++ Class** wizards (*Simple Symbian Style C++ Class* = two-phase skeleton; *Active Object Class*).
* Code templates: type `lit` then `Ctrl+Space` for `_LIT`, plus templates for cleanup-stack patterns, loops (*Window → Preferences → C/C++ → Editor → Templates*).
* Handy keys: `Ctrl+Space` content assist, `F3` open declaration, `Ctrl+/` toggle comment, `Ctrl+Shift+F` format, `Ctrl+I` indent, `Ctrl+Shift+N` add include, `Ctrl+F11` run, `F11` debug.
* **SIS Builder** (project properties → Carbide.c++ → Build Configurations → SIS Builder): choose the `.pkg`, output name, signing (self-signed by default, or your `.cer/.key`); builds `.sisx` automatically after each phone build. `Build PKG File` on a `.pkg` does it on demand.
* *Project → Freeze Exports* = `abld freeze`.
* Professional extras: Performance Investigator (profiler), Crash Debugger, Dependency Explorer, CodeScanner (static analysis of Symbian idioms — very useful: catches leaving in constructors, missing `Cancel()`, `_L` usage, etc.).

### Debugging
* **Emulator debugging:** pick the *Emulator Debug* configuration and press `F11`. The Debug perspective shows threads (the emulator is one process), Variables, Breakpoints, **Symbian OS Data** (processes, threads, chunks, libraries), Registers, Modules. Breakpoints can have conditions/actions (log, play sound, resume).
* **On-device debugging with TRK** (Target Resident Kernel): install the signed **App TRK** (`s60_3_x_app_trk_*.sisx`, shipped with Carbide; must match the platform), start TRK on the phone, connect via **USB** (PC Suite/Ovi Suite drivers, "PC Suite" mode, select the virtual COM port) or **Bluetooth** (serial port profile), then debug the *Phone Debug (GCCE/ARMV5)* configuration with an *Application TRK* launch configuration — Carbide builds, packages, installs and starts the app. TRK must stay running in the background. *System TRK* (needs special signing) allows debugging system components.
* Without a debugger: `RDebug::Print(_L("x=%d"), x)` → emulator `EPOCWIND.OUT` / IDE console; on devices use a file logger (`RFileLogger`, `flogger.lib`, writes to `C:\Logs\<dir>\` if the directory exists) or your own log file; enable panic details with `C:\resource\ErrRd`. Tools of the era: Y-Browser/Y-Tasks, Nokia Energy Profiler, `ProfilerTRK`.
* Nokia **Remote Device Access** (RDA) once offered remote real devices; discontinued.

## 6.2 After 2013: the situation today

* **Symbian Signed closed** (last Certified Signed submissions 13 Dec 2013; service closed early January 2014); the Nokia Store stopped accepting Symbian content on 1 January 2014. Self-signing and side-loading still work (see [Signing](23-signing-certification.md)).
* Nokia/Forum Nokia/Symbian Foundation websites (SDK downloads, wiki, forums) are offline; content is partly preserved on the Internet Archive and community mirrors.
* The Symbian Foundation's open-source code (EPL, 2010) was withdrawn from the official site in 2011 but copies circulate — useful as reference for how system APIs behave.

### What hobbyists use now
| Tool | Purpose |
|------|---------|
| Archived **S60 3rd/5th Ed. and Symbian^3 / Nokia Symbian (Belle) SDKs**, **Carbide.c++ 2.x/3.2**, **Qt SDK 1.x** (Qt 4.7/4.8 for Symbian, with Qt Creator and a remote compiler/local GCCE) | The original environment, often run in a 32-bit Windows XP/7 VM to avoid installer and Perl problems |
| **GCCE 4.x builds** (e.g. the *GCC4Symbian* / *gcce4symbian* projects) | Newer GCC for Symbian EABI targets (C++ features, better codegen); used at your own risk with the classic build scripts |
| Community build scripts (CMake/Make wrappers around `elf2e32`, `makesis`, `signsis`), Linux ports of the SDK tools (from the open-sourced tool code) | Building without the Windows SDK installer |
| **EKA2L1** | Open-source emulator of the Symbian kernel/N-Gage 2.0 that runs real S60v1–v5 apps on modern PCs and Android — great for testing |
| Python for S60, Open C/C++, Qt | Alternative languages/runtimes on the device |
| Community app catalogues and "hacked" devices (installserver patches) | Distributing and installing self-signed software with extra capabilities on your own phones |

Practical advice:
* Use a VM with Windows XP/7 32-bit, ActivePerl 5.6.1, the SDK on `C:\Symbian\…`, and projects on the same drive without spaces in paths — this avoids most old-tool breakage.
* Build GCCE targets (RVCT licenses are rarely available).
* Test on real hardware or EKA2L1; the SDK emulator often fails on modern Windows.
* Sign with a self-generated certificate; restrict capabilities to user capabilities unless you control the target device's security policy.

## See also
* [SDK and build](05-sdk-and-build.md)
* [References](../REFERENCES.md)

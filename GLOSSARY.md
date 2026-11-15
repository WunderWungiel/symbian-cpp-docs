# Glossary

| Term | Meaning |
|------|---------|
| **ABI / EABI** | (Embedded) Application Binary Interface. Symbian 9.x device binaries use ARM EABI v2 (GCCE, RVCT). |
| **abld** | Generated build script (`abld.bat`) driving the build steps (export, makefile, library, resource, target, final). |
| **Active object (AO)** | A `CActive`-derived object that issues one asynchronous request and handles its completion in `RunL()`. |
| **Active scheduler** | `CActiveScheduler`: per-thread event loop dispatching AO completions. |
| **AIF** | Application Information File (pre-9 app icons/captions); replaced by registration files + MIF in 9.x. |
| **AllFiles, TCB, DRM** | Manufacturer capabilities. |
| **AppArc** | Application Architecture: app registry, launching, MIME associations (`apsexe.exe`, SID `0x10003A3F`). |
| **AppUi** | The event-handling class of a GUI app (`CAknAppUi`). |
| **ASD** | Accredited Symbian Developer, the certification exam of the era. |
| **Avkon** | S60's UI toolkit built on Uikon. |
| **BAFL** | Basic Application Framework Library (`bafl.lib`): descriptor arrays, `BaflUtils`, resource files. |
| **bld.inf** | Project component list. |
| **bmconv** | Tool that builds `.mbm` multi-bitmap files from BMPs. |
| **C class** | Heap class derived from `CBase`: zero-filled, virtual destructor, non-copyable. |
| **Capability** | Permission token compiled into a binary, checked by servers/kernel. |
| **Carbide.c++** | Nokia's Eclipse-based IDE for Symbian. |
| **CBA** | Command Button Array: the softkeys (`R_AVKON_SOFTKEYS_OPTIONS_EXIT`). |
| **Central Repository** | System key–value settings store with change notification (`CRepository`). |
| **Chunk** | A contiguous memory region (`RChunk`), possibly shared between processes. |
| **Cleanup stack** | Per-thread stack of objects to destroy if a leave happens. |
| **CommsDat / CommDB** | Database of communications settings (access points/IAPs). |
| **CONE** | Control Environment: `CCoeEnv`, `CCoeControl`, `CCoeAppUi`. |
| **Data caging** | File-system protection of `\sys\`, `\resource\`, `\private\`. |
| **DBMS** | Symbian's relational database engine (`edbms.lib`). |
| **.def file** | Frozen list of DLL exports (ordinals). |
| **Descriptor** | Symbian string/buffer class (`TDesC` family). |
| **DevCert** | Developer certificate: IMEI-locked, grants extra capabilities. |
| **DSA** | Direct Screen Access. |
| **E32Image** | The Symbian executable format produced by `elf2e32`. |
| **E32Main** | Entry point of every EXE. |
| **ECom** | Plug-in framework (`REComSession`). |
| **EKA1 / EKA2** | EPOC Kernel Architecture 1 / 2. EKA2: real-time, multithreaded kernel of 8.1b and 9.x. |
| **Emulator (WINS/WINSCW)** | Windows-hosted Symbian environment from the SDK. |
| **EPOC** | Psion's predecessor of Symbian OS. |
| **EPOCROOT** | Environment variable: root of the active SDK (`\Symbian\9.x\S60_3rd_xx\`). |
| **ErrRd** | Empty file `C:\resource\ErrRd` that makes the phone show panic details. |
| **ESock** | Socket server. |
| **ETel** | Telephony server; `CTelephony` is its third-party API. |
| **EUserHL** | Library of L-classes (`LString`, `LCleanedupPtr`…). |
| **FBSERV** | Font and Bitmap Server. |
| **FEP** | Front-End Processor: input methods (T9, handwriting). |
| **FP** | Feature Pack (S60 version increment within an edition). |
| **GCCE** | GCC for Embedded; the free ARM compiler target. |
| **HAL** | Hardware Abstraction Layer (`HAL::Get()`). |
| **HBufC** | Heap-allocated, non-modifiable descriptor. |
| **IAP** | Internet Access Point. |
| **IPC / ITC** | Inter-process / inter-thread communication. |
| **L class** | "Liberal with leaving" self-managing class from EUserHL. |
| **LAF** | Look-and-Feel library used by Uikon. |
| **LDD / PDD** | Logical / physical device driver. |
| **Leave** | Symbian exception (`User::Leave()`), caught by TRAP. |
| **LIT (`_LIT`)** | Macro defining a literal descriptor constant. |
| **M class** | Mixin interface (pure virtual functions, no data). |
| **makesis / signsis / makekeys** | Tools to build, sign and create keys for SIS packages. |
| **MBM / MIF** | Multi-bitmap file / multi-icon (SVG) file. |
| **mifconv** | Tool that builds `.mif` from SVG/BMP. |
| **MMF** | Multimedia Framework. |
| **MMP** | Project file describing one binary. |
| **MTM** | Message Type Module (messaging plug-in). |
| **Open C / P.I.P.S.** | POSIX libraries for Symbian (libc, libpthread, libm…). |
| **Panic** | Unrecoverable error that terminates the thread (category + reason). |
| **PID** | Package UID in a PKG file. |
| **PKG** | Script describing a SIS package. |
| **PlatSec** | Platform Security (9.x). |
| **Polymorphic DLL** | DLL exposing one factory function returning an object implementing an interface. |
| **PRT** | Socket protocol plug-in module. |
| **P&S** | Publish & Subscribe (`RProperty`). |
| **R class** | Handle class owning an external resource; explicit `Close()`. |
| **RCOMP** | Resource compiler. |
| **Recognizer** | Plug-in that determines a file's MIME type. |
| **Rendezvous** | `RProcess/RThread::Rendezvous()` — "I'm ready" signal. |
| **ROFS** | Read-Only File System (part of the ROM image on NAND devices). |
| **RSC / RSG / RSS / RH / HRH** | Compiled resource / generated resource IDs / resource source / resource header / shared C++ and resource header. |
| **RVCT** | ARM RealView Compilation Tools (commercial compiler, target ARMV5). |
| **S60** | Nokia's Symbian UI platform (Series 60). |
| **SID / VID** | Secure ID (process identity, defaults to UID3) / Vendor ID. |
| **SIS / SISX** | Installation package / signed SIS (conventional extension). |
| **SLM** | Startup List Management (autostart). |
| **Stray signal** | A completed request with no matching active AO (`E32USER-CBase 46`). |
| **SVG-T** | SVG Tiny, used for scalable icons. |
| **T class** | Value type: no destructor, owns nothing. |
| **TCB / TCE** | Trusted Computing Base / Trusted Computing Environment. |
| **TLS** | Thread-Local Storage (`Dll::Tls()`), used to avoid WSD. |
| **TRAP / TRAPD / TRAP_IGNORE** | Macros that catch leaves. |
| **TRK** | Target Resident Kernel: on-device debug agent. |
| **Twip** | 1/20 point = 1/1440 inch; device-independent unit. |
| **UDEB / UREL** | Debug / release build variants. |
| **UID1/2/3** | The three type identifiers in a file header. |
| **UIQ** | Sony Ericsson/Motorola touch UI platform (discontinued 2009). |
| **Uikon / Eikon** | Generic GUI framework under Avkon/Qikon. |
| **View Server** | Manages app views; panics `ViewSrv 11` on unresponsive apps. |
| **WINSCW** | Emulator build target (CodeWarrior x86 compiler). |
| **WSD** | Writable static data (global/static variables), problematic in DLLs. |
| **WSERV** | Window Server. |
| **XIP** | eXecute In Place: running code directly from ROM. |

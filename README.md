# Symbian C++ Documentation

A complete, practical reference for **native Symbian C++ development** (Symbian OS 9.x: S60 3rd/5th Edition, Symbian^1/^3/Anna/Belle), written for **humans and for LLM agents**.

It covers the platform, security model, project and build system, the language idioms that make Symbian C++ different from standard C++ (leaves, cleanup stack, two-phase construction, descriptors, active objects), and the main system APIs (files, streams, databases, IPC, client–server, ECom, sockets, window server, graphics), packaging and signing.

> **Status of the platform:** Symbian is discontinued. Nokia stopped Symbian Signed and Store submissions at the start of 2014, and the official SDK download sites are offline. This documentation describes the platform as it was and notes where things have changed since. See [IDEs and modern toolchains](docs/06-ide-and-toolchains.md) for the current situation.

## How to read this

* **New to Symbian?** Read 1 → 2 → 3 → 5 → 7 → 8 → 9 → 11 → 12 in order. Those chapters cover everything you need for a first app.
* **Looking something up?** Use the table of contents below or the [Glossary](GLOSSARY.md).
* **LLM / coding agent?** Start with [AGENTS.md](AGENTS.md). It holds the hard rules, checklists and common mistakes, and points to the right chapter for each task. [llms.txt](llms.txt) is a machine-readable index.

## Table of contents

| # | Chapter | Topics |
|---|---------|--------|
| 1 | [Platform overview](docs/01-platform-overview.md) | History, OS vs. UI platform (S60/UIQ), versions, architecture, EKA2, ROM/RAM, drives |
| 2 | [Platform security](docs/02-platform-security.md) | UIDs, SID/VID, capabilities, trust levels, data caging, signing basics |
| 3 | [Project structure](docs/03-project-structure.md) | Folder layout, `bld.inf`, MMP keywords, binary naming |
| 4 | [Resource files](docs/04-resource-files.md) | `.rss`/`.rh`/`.rsg`, `STRUCT`/`RESOURCE`, IDs, localization |
| 5 | [SDK and build](docs/05-sdk-and-build.md) | SDKs, `devices`, compilers/targets, `bldmake`/`abld`, `.def` freezing, emulator, PKG, `makesis`/`signsis`, install errors |
| 6 | [IDEs and toolchains](docs/06-ide-and-toolchains.md) | Carbide.c++, TRK debugging, logging, the situation today |
| 7 | [Fundamentals](docs/07-fundamentals.md) | Types, naming conventions, T/C/R/M classes, two-phase construction |
| 8 | [Errors and cleanup](docs/08-errors-leaves-cleanup.md) | Error codes, leaves, TRAP, panics, asserts, heap/stack, leak checks, cleanup stack, L-classes |
| 9 | [Descriptors](docs/09-descriptors.md) | All descriptor classes, literals, formatting, conversion, packages, `TLex` |
| 10 | [Arrays](docs/10-arrays.md) | `RArray`, `RPointerArray`, `CArrayX`, descriptor arrays, `TFixedArray` |
| 11 | [Active objects](docs/11-active-objects.md) | `CActive`, scheduler, cancellation, long tasks, nested waits, stray signals |
| 12 | [Applications](docs/12-applications.md) | Console EXE, S60 GUI architecture, registration, icons (MIF), UIDs, autostart |
| 13 | [DLLs and libraries](docs/13-dlls-libraries.md) | Static libs, shared DLLs, exports, polymorphic DLLs, WSD |
| 14 | [Processes, threads, IPC](docs/14-processes-threads-ipc.md) | `RProcess`, `RThread`, mutexes, semaphores, chunks, message queues, Publish & Subscribe |
| 15 | [Client–server](docs/15-client-server.md) | `CServer2`, `CSession2`, `RSessionBase`, `RMessage2`, transient servers, security |
| 16 | [ECom and recognizers](docs/16-ecom.md) | Interfaces, plug-ins, registration, resolvers, MIME recognizers |
| 17 | [Time and timers](docs/17-time-timers.md) | `TTime`, `TDateTime`, formatting, `RTimer`, `CTimer`, `CPeriodic`, `CHeartbeat` |
| 18 | [File system](docs/18-file-system.md) | `RFs`, `TParse`, private dirs, drives, `CFileMan`, `RFile`, `TFileText` |
| 19 | [Streams and stores](docs/19-streams-stores.md) | Read/write streams, `ExternalizeL`, file stores, dictionaries |
| 20 | [Databases](docs/20-databases.md) | DBMS: tables, indexes, cursors, SQL views, transactions; Symbian SQL |
| 21 | [Sockets and networking](docs/21-sockets-networking.md) | ESock, `RSocket`, `RHostResolver`, Bluetooth discovery, `RConnection`, HTTP pointers |
| 22 | [Window server and graphics](docs/22-window-server-graphics.md) | WSERV, window groups, key events and capture, redraws, `CWindowGc`, fonts, bitmaps |
| 23 | [Signing and certification](docs/23-signing-certification.md) | Symbian Signed programs, Publisher ID, test criteria, today's options |
| — | [Glossary](GLOSSARY.md) | Acronyms and terms |
| — | [References](REFERENCES.md) | Books, papers, archives, communities |
| — | [Examples](examples/) | Buildable example projects |

## Conventions used here

* Code follows Symbian naming conventions (`iMember`, `aArgument`, `KConstant`, `EEnumValue`, trailing `L`/`LC`/`D`).
* "S60 3rd Edition" means Symbian OS 9.1, "FP1" 9.2, "FP2" 9.3, "5th Edition" 9.4. See [chapter 1](docs/01-platform-overview.md#12-operating-system-vs-ui-platform).
* Shell commands are for the Windows SDK command prompt.
* `_L("...")` appears in a few short snippets for brevity. In real code use `_LIT` (see [Descriptors](docs/09-descriptors.md#93-literals-_lit-and-_l)).

## Sources

This documentation draws on:

* The Russian-language book *Symbian C++. Программирование для мобильных телефонов* (Williams Publishing, 2010). It was the primary source for structure and content. The material was translated, reorganised and expanded, not copied. The book's PDF is **not** part of this repository.
* Official Symbian/Nokia SDK documentation of the era, plus *Symbian OS Explained* (J. Stichbury) and the *Accredited Symbian Developer Primer*, as cited in [REFERENCES.md](REFERENCES.md).
* Public information about the platform's later history (2010–2014) and today's community tools.

Corrections and additions are welcome. Symbian behaved slightly differently across versions and devices, so whenever possible check against a real device or the SDK help for your target.

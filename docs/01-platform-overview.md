# 1. Symbian OS: Platform Overview

> **Scope:** history, OS vs. UI platform, architecture, the EKA2 kernel, executables in ROM/RAM.
> **Applies to:** Symbian OS 9.x (S60 3rd/5th Edition, Symbian^1/^3). Most concepts also apply to earlier versions.

## 1.1 Short history

| Year | Milestone |
|------|-----------|
| 1989 | Psion ships **EPOC** (16-bit, later called EPOC/16 or SIBO) for its organisers. Written in 8086 assembly and C. |
| 1997 | **EPOC/32** (renamed EPOC) on the Psion Series 5 — ARM-based, applications written in **C++**. Releases ER2, ER3 (there was no ER4). |
| 1998 | Psion Software, Nokia and Ericsson found **Symbian Ltd.** |
| 1999–2000 | EPOC Release 5 / 5u ("Symbian 5.0/5.1"). From 5u on, strings are **Unicode by default**. Ericsson R380. |
| 2000–2001 | Symbian OS 6.0 / 6.1. Nokia 9210 (Series 80) and Nokia 7650 (Series 60). |
| 2002–2003 | Symbian 7.0 / 7.0s. UIQ (touch-screen platform) appears. |
| 2004 | Symbian 8.1a and **8.1b, with the new EKA2 kernel**. |
| 2005 | **Symbian OS 9.1** introduces **Platform Security** — installed software must be signed. Binary compatibility with 8.x and earlier is broken. |
| 2006 | Symbian 9.2 / 9.3: demand paging, built-in Wi-Fi and HSDPA. |
| 2007 | POSIX libraries (Open C / P.I.P.S.) and SQLite ported. |
| 2008 | Nokia buys Symbian Ltd.; the **Symbian Foundation** is created to open-source the OS. S60, UIQ and MOAP(S) code contributed. |
| 2009 | UIQ discontinued. S60 5th Edition (Symbian 9.4) becomes **Symbian^1**. EKA2 kernel source published (Oct 2009). |
| 2010–2013 | (from public sources) Symbian^3 (Nokia N8, 2010), then Symbian Anna and **Nokia Belle** (2011–2012). Nokia moves to Windows Phone in 2011; Accenture takes over Symbian maintenance; the last Symbian device (Nokia 808 PureView) ships in 2012; Nokia ends Symbian support and the Symbian Signed / Nokia Store infrastructure is wound down around 2013–2014. |

Over its lifetime, Symbian shipped on hundreds of device models; in early 2009 it held roughly 46% of the world smartphone market.

## 1.2 Operating system vs. UI platform

Until 2008 Symbian Ltd. licensed **the OS** (kernel, services, a minimal GUI framework) to phone makers, who each built **a UI platform** on top: their own UI toolkit, extra services, pre-installed apps and Java integration. Applications written for one platform were usually **not** compatible with another.

| Platform | Vendor | Notes |
|----------|--------|-------|
| **S60** (Series 60) | Nokia (also Samsung, LG, Sony Ericsson) | By far the most common; UI toolkit **Avkon** |
| Series 80, Series 90 | Nokia | Communicators / Nokia 7710 |
| **UIQ** | Sony Ericsson, Motorola | Pen-based; UI toolkit **Qikon**. Discontinued 2009 |
| MOAP(S) | NTT DoCoMo (Japan) | Closed — no native 3rd-party apps |

Nokia versions S60 by **editions** and **Feature Packs (FP)**:

| S60 release | Symbian OS |
|-------------|-----------|
| S60 0.9 | 6.1 |
| S60 1st Edition (1.2) | 6.1 |
| S60 2nd Edition (2.0) | 7.0s |
| S60 2nd Ed. FP1 (2.1) | 7.0s |
| S60 2nd Ed. FP2 (2.6) | 8.0a |
| S60 2nd Ed. FP3 (2.8) | 8.1a |
| **S60 3rd Edition (3.0)** | **9.1** |
| S60 3rd Ed. FP1 (3.1) | 9.2 |
| S60 3rd Ed. FP2 (3.2) | 9.3 |
| **S60 5th Edition (5.0)** = Symbian^1 | **9.4** |
| Symbian^3 / Anna / Belle | 9.5-based ("Symbian^3") |

UIQ 3.0/3.1/3.2 run on Symbian 9.1–9.3.

> **Binary compatibility rule of thumb:** an application built for S60 3rd Edition (9.1) runs on all later S60 3rd/5th Edition and Symbian^3 devices unless it uses newer APIs. Code built for S60 2nd Edition (pre-9.x) does **not** run on 9.x and must be ported.

With the Symbian Foundation the "platform" layer was merged into the OS, so "Symbian^1" is both the OS and the UI. In this documentation, "S60" means the Nokia UI layer (Avkon etc.) and "Symbian OS" means the layers beneath it.

## 1.3 Architecture

Symbian OS is a **single-user, multitasking, real-time OS with a microkernel architecture**. Almost every system service is a **server** running in its own process; applications are **clients** that talk to servers over IPC. Most services can be extended by **plug-ins** (polymorphic DLLs, usually ECom plug-ins).

```
+---------------------------------------------------------------+
| Applications (yours, built-in apps, Java MIDlets)             |
+---------------------------------------------------------------+
| UI platform: S60 (Avkon) / UIQ (Qikon)                        |
+---------------------------------------------------------------+
| Application services: PIM, Messaging, SyncML, Java ME         |
| Generic middleware:  app support, security, HTTP/WAP,         |
|                      multimedia middleware, GUI framework     |
| OS services: base, comms, multimedia, graphics, location      |
+---------------------------------------------------------------+
| Kernel services & HAL (EKA2)                                  |
+---------------------------------------------------------------+
| Hardware (ARM CPU, baseband, peripherals)                     |
+---------------------------------------------------------------+
```

### Kernel: EKA2
* **EKA2** ("EPOC Kernel Architecture 2") is used by all Symbian 9.x devices (first shipped in 8.1b). EKA1 was single-threaded; EKA2 is **pre-emptively multithreaded** and **hard real-time** — fast enough to run the GSM/WCDMA/CDMA signalling stack on the same CPU.
* Responsibilities: processes and threads (creation, scheduling), memory management, interrupts, timers, mutexes, semaphores, a two-level **LDD/PDD** (logical/physical device driver) model, IPC, and the kernel side of platform security.
* The kernel does **not** use leaves/cleanup (see [Errors and cleanup](08-errors-leaves-cleanup.md)).

### Base services (user mode)
| Component | Role |
|-----------|------|
| **User library** (`euser.dll`) | The core API: processes, threads, mutexes, message queues, basic types, dates/times, timers, arrays, lists, **active objects**, **cleanup stack**, **descriptors**, UIDs, client/server framework, Publish & Subscribe. Linked by practically everything. |
| **File server** (`efsrv.dll`, F32) | First server started after the kernel; file-system plug-ins (FAT, ROFS…). |
| **DBMS** (`edbms.dll`) | Relational databases in a single file or stream, with an SQL subset. (Later also Symbian SQL / SQLite.) |
| **Central Repository** | Key/value store for system and application settings, with change notifications. |
| **ECom** | Discovering and loading plug-in implementations of interfaces. |
| **BAFL** (`bafl.dll`) | Basic Application Framework Library: descriptor arrays, file-name utilities, etc. |

### Communications services
* **C32 Serial Server** – serial-style access to RS-232, IrDA, Bluetooth, USB; extended by **CSY** plug-ins.
* **ETel Telephony Server** – phones, lines, calls; extended by **TSY** plug-ins (MultiMode TSY, CDMA TSY, SIM TSY). Third-party apps use the restricted **ETel 3rd-party API** (`CTelephony`).
* **ESock Socket Server** – socket API; protocols are **PRT** plug-ins (TCP/IP, Bluetooth, IrDA, SMS…).
* Fax server, and the **Comms Root Server** that starts/stops the comms servers.
* Supported technologies: RS-232, IrDA, USB, Bluetooth, Wi-Fi, SMS, MMS, WAP, OBEX, SIP, TCP/IP, SyncML, 2G/2.5G/3G.

### Other OS services
* **Multimedia** – a unified framework (MMF) for audio, video, MIDI, camera, speech; codecs are ECom plug-ins.
* **Graphics** – the **Window Server**, GDI and BitGDI; bitmap and FreeType vector fonts, right-to-left scripts; **OpenGL ES** for 3D.
* **Location (LBS)** – the Location Server with **PSY** positioning plug-ins (GPS, A-GPS, network); landmarks database.

### Middleware
* Application support: converters, text formatting, Timezone server, Alarm server, backup.
* Security: cryptography, SIS and JAR installers.
* Protocols: HTTP, WAP, WAP Push, Bluetooth profiles; MTP, RTP, SIP.
* **GUI framework**:
  * **CONE** (Control Environment) – base classes for controls; routes Window Server events to controls.
  * **Uikon** – the generic application framework (Application / Document / AppUi / View) and standard controls; base for **Avkon** (S60) and **Qikon** (UIQ). Loads the platform's **LAF** (look-and-feel) library.
  * **FEP** (Front-End Processor) – input pre-processing: handwriting recognition, multi-tap, predictive text (T9).

### Application services
PIM (vCard/vCalendar/iCalendar, contacts DB, calendar), Messaging (Message Server + **MTM** plug-ins for SMS, MMS, POP3/IMAP4/SMTP, OBEX; BIO messages), SyncML device management, and Java ME (MIDP 2.0 / CLDC 1.1).

## 1.4 Executables in ROM and RAM

Devices have two kinds of storage:

* **ROM** – drive `Z:`; written at manufacture. EXEs and DLLs **execute in place** (XIP) from ROM; only their writable data is copied to RAM. Relocation information is stripped to save space, so **a DLL copied out of ROM cannot be loaded from RAM**.
* **RAM / flash** – internal drive `C:` and removable memory card (usually `E:`). User-installed binaries live here and are **loaded fully into RAM** before execution. If several processes use the same EXE or DLL, its **code and read-only data are shared**; only writable sections are duplicated. The binary is unloaded when the last user exits.

(Symbian 9.3+ adds **demand paging**, where ROM/ROFS code is paged in as needed.)

## 1.5 Typical drive letters

| Drive | Meaning |
|-------|---------|
| `C:` | Internal user storage (phone memory) |
| `D:` | RAM disk (temporary, lost on reboot) |
| `E:` | Memory card (often; on some devices mass memory, with `F:` the card) |
| `Z:` | ROM (read-only) |

Never hard-code `C:` for files you install — the user may install your app on a memory card. Get your own drive from the running process (see [File system](18-file-system.md)).

## See also
* [Platform security, UIDs, capabilities](02-platform-security.md)
* [Glossary](../GLOSSARY.md)

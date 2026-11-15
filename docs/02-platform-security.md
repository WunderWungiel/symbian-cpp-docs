# 2. Platform Security: UIDs, Capabilities, Data Caging, Signing

> **Scope:** the security model introduced in Symbian OS 9.1 ("PlatSec") — the rules every 9.x binary must obey.
> **Key idea:** *the process is the unit of trust and the unit of memory protection.*

## 2.1 UIDs

A **UID** is a 32-bit unsigned number, usually written in hex (`0x00000000`–`0xFFFFFFFF`). UIDs identify objects, declare that something implements an interface, and associate documents with applications.

Every executable and many data files start with a **12-byte header** holding three UIDs (plus a checksum):

| UID | Meaning |
|-----|---------|
| **UID1** | File type: EXE, DLL, file store, resource file… Set **indirectly** via `TARGETTYPE` in the MMP file (`EXE` → `0x1000007A`, `DLL` → `0x10000079`). |
| **UID2** | Depends on UID1. Ignored for plain EXEs. For polymorphic DLLs it names the **interface** implemented (e.g. `0x10009D8D` = ECom plug-in, `0x100039CE` = pre-9.x GUI app). |
| **UID3** | Distinguishes files with the same UID1/UID2 — effectively **the application's unique ID**. |

Executables additionally carry:

* **SID (Secure ID)** – identifies the process. Set with `SECUREID` in the MMP; **defaults to UID3** (recommended: don't set it separately). The installer refuses to install a second executable with a SID already present on the device. Determines the process's **private directory** `\private\<SID>\`. Servers can restrict clients by SID. Ignored for DLLs.
* **VID (Vendor ID)** – identifies the author (e.g. Nokia = `0x101FB657`). Set with `VENDORID`; default `0`. A non-zero VID requires the binary to be signed through Symbian Signed. Ignored for DLLs. Third-party developers normally leave it at 0.

### UID ranges

| Range | Protected? | Use |
|-------|-----------|-----|
| `0x00000000`–`0x0FFFFFFF` | — | Development only (do not ship) |
| `0x10000000`–`0x1FFFFFFF` | protected | Legacy UIDs allocated before 9.x |
| `0x20000000`–`0x2FFFFFFF` | **protected** | **Allocated UIDs for signed apps (9.x)** |
| `0x30000000`–`0x6FFFFFFF` | protected | Reserved |
| `0x70000000`–`0x7FFFFFFF` | protected | VIDs |
| `0x80000000`–`0x9FFFFFFF` | unprotected | Reserved |
| `0xA0000000`–`0xAFFFFFFF` | unprotected | Allocated UIDs for **self-signed** apps (9.x) |
| `0xB0000000`–`0xDFFFFFFF` | unprotected | Reserved |
| `0xE0000000`–`0xEFFFFFFF` | unprotected | **Development/test** UIDs (pick at random; required for Open Signed Online) |
| `0xF0000000`–`0xFFFFFFFF` | unprotected | Legacy compatibility |

*Protected* UIDs were obtained from Symbian Signed and recorded against the developer, so they were guaranteed unique and traceable. The installer only accepts a protected UID3/SID in a package signed with a trusted certificate. Unprotected UIDs (`0xE…`) are for examples and testing.

> **Practical rule:** while developing, use a random `0xE0000000`–`0xEFFFFFFF` UID3. Today, with Symbian Signed gone, hobbyist projects simply pick unprotected UIDs (`0xA…`/`0xE…`), or use protected ones on devices with the installer's checks patched out.

## 2.2 Capabilities

Access to sensitive APIs is guarded by **capabilities** — labels compiled into each binary's header (`CAPABILITY` in the MMP). About 40% of the 9.x APIs are capability-protected; **most APIs need no capability at all.** There are 20 capabilities (plus `None`) in four groups, listed by increasing sensitivity:

### User capabilities (user can grant; self-signing allowed)
| Capability | Grants |
|-----------|--------|
| `LocalServices` | Bluetooth, IrDA, USB and other local connectivity |
| `Location` | Device location (GPS, A-GPS). *System capability on S60 3rd Ed. and FP1* — not in `swipolicy.ini` there, so self-signed packages declaring it fail to install. |
| `NetworkServices` | Network access that may cost money: GPRS/3G/WLAN data, calls, SMS |
| `ReadUserData` | Reading user's private data (contacts, calendar, messages…) |
| `UserEnvironment` | Camera, microphone/audio recording |
| `WriteUserData` | Modifying user's private data |

### System capabilities (developer certificate / Open Signed / Express Signed)
| Capability | Grants |
|-----------|--------|
| `PowerMgmt` | Kill other processes, power down the device |
| `ProtServ` | Register a server with a protected name (starting with `!`); set system/protected status |
| `ReadDeviceData` | Read device settings and network info (e.g. IMSI) |
| `SurroundingsDD` | Logical device drivers giving environment information (e.g. positioning hardware) |
| `SwEvent` | Capture and simulate key and pointer events (e.g. read keys while in background) |
| `TrustedUI` | Show dialogs in a secure UI environment |
| `WriteDeviceData` | Change device settings and system service configuration |

### Restricted capabilities (Certified Signed only)
| Capability | Grants |
|-----------|--------|
| `CommDD` | Direct access to communications device drivers (USB, Wi-Fi, serial) |
| `DiskAdmin` | Format, check, mount drives, set passwords |
| `NetworkControl` | Low-level network/telephony control (ETel server, comms root server, SMS/USSD) |
| `MultimediaDD` | Low-level multimedia access (audio/video drivers, priority) |

### Manufacturer capabilities (device manufacturer approval)
| Capability | Grants |
|-----------|--------|
| `AllFiles` | Read the whole file system incl. `\sys\` and other apps' `\private\`; file-change notifications on any path |
| `DRM` | Access to unprotected DRM content |
| `TCB` | Write access to `\sys\` and `\resource\`; the Trusted Computing Base itself |

Example: `CTelephony::GetSubscriberId()` (IMSI) requires `ReadDeviceData`. Without it the call fails with `KErrPermissionDenied` (-46) or `KErrAccessDenied` (-21). The SDK reference lists the required capabilities for each protected API. Some APIs need several capabilities, and the requirement can depend on which resources or plug-ins are involved.

### Capability rules
1. **A process gets the capabilities of its EXE.** They cannot change at run time.
2. **DLL loading rule:** a process may only load a DLL whose capability set is **equal to or a superset of** the process's capabilities. This applies recursively to every DLL that DLL links to. If a DLL can't be loaded, a statically-linked EXE simply won't start (error `KErrPermissionDenied`).
   → Shared DLLs and plug-ins therefore usually declare **more** capabilities than they need (often `CAPABILITY ALL -TCB`) so any client can load them.
3. A DLL's own capabilities don't grant it anything; it runs with the loading process's capabilities.
4. Capabilities must be certified by the signing certificate at install time (see below).

## 2.3 Trust levels

| Level | Members | Capabilities |
|-------|---------|-------------|
| **TCB** — Trusted Computing Base | Kernel, file server, software installer, some drivers, hardware | All. Kept minimal. |
| **TCE** — Trusted Computing Environment | System servers (window server, socket server, ETel…) | Only what they need |
| **Applications** | Pre-installed and user-installed | **Trusted** if signed with a certificate granting system or higher caps; otherwise **untrusted** (user caps only, sandboxed — e.g. cannot autostart at boot on many devices) |

## 2.4 Data caging

The file server (part of the TCB) checks every file operation against the caller's capabilities. Three directory trees on **every drive** are caged:

| Path | Read | Write | Contents |
|------|------|-------|----------|
| `\sys\` | `AllFiles` | `TCB` | `\sys\bin\` — **all** executables (EXE, DLL); `\sys\hash\` etc. |
| `\resource\` | anyone | `TCB` | Read-only shared resources: `\resource\apps\` (app resource files, icons) |
| `\private\<SID>\` | owner process or `AllFiles` | owner process or `AllFiles` | The process's private data. `<SID>` is 8 hex digits without `0x`. |
| `\private\<SID>\import\` | — | installer | Other packages may drop files here at install time (plug-in registration) |
| everything else | anyone | anyone | Public data (`C:\Data\...`) |

Consequences:

* Executables **must** be in `\sys\bin\`; the loader looks nowhere else. Because the directory is flat, **binary names must be globally unique** — in practice embed the UID in the file name for DLLs (e.g. `mylib_0xE1234567.dll`).
* Files under `\private\<SID>\` are removed automatically on uninstall.
* **DLLs have no private directory**; they use the private directory of the process that loaded them.
* Well-known import directories:
  * `\private\10003a3f\import\apps\` – application **registration files** (`*_reg.rsc`), read by the AppArc server (`apsexe.exe`, SID `0x10003A3F`). Required for the app to appear in the menu.
  * `\private\101f875a\import\` – **startup list** files for autostart (`[UID].rsc`), read by the Startup List Management / installer (SID `0x101F875A`).
* On removable media, binaries' hashes are stored in `C:\sys\hash\` at install time so tampering on a PC is detected. A package can also forbid installation on removable media.

## 2.5 Installation and signing

Users cannot write to `\sys\bin\`, so **the only way to install native code is the software installer** (a TCB component), from a **SIS/SISX package** (see [SDK and build](05-sdk-and-build.md)).

Before installation a package is **signed** with one or more **digital certificates** containing: issuer, subject, validity period, optionally a list of allowed **IMEIs**, and the **set of capabilities** it vouches for. **Unsigned SIS files cannot be installed on 9.x.**

The installer:
1. Rejects packages without a valid (unexpired, optionally not revoked via OCSP) certificate chain. The expiry only matters **at install time**; installed software keeps working.
2. Computes the union of capabilities declared by all executables in the package and compares it with the union of capabilities granted by the certificates. If any declared capability isn't granted, installation fails. **User capabilities** are exempt (they are listed in `\system\data\swipolicy.ini`, which the manufacturer controls) — the user is simply asked.

Certificate types:

| Certificate | Who | Capabilities | Install on | Trusted? |
|-------------|-----|-------------|------------|----------|
| **Self-signed** (self-generated with `makekeys`) | Anyone | User caps only | Any device (user warned) | Untrusted |
| **Developer certificate (DevCert)** | Via Open Signed Online/Offline | User + system (+restricted with extra approval) | Only listed IMEIs | Trusted (test only) |
| **Symbian Signed** (Express / Certified) | After testing by Symbian Signed | All needed except manufacturer (with approval) | Any device | Trusted |

* **Open Signed Online** – upload a SIS, enter one IMEI and choose caps from user+system; receive the signed SIS by e-mail. UID3 must be in `0xE0000000`–`0xEFFFFFFF`. You never receive the certificate itself.
* **Open Signed Offline** – needs a **Publisher ID** (an ACS certificate from TC TrustCenter/VeriSign). You get a DevCert file for up to 1000 IMEIs and any capabilities except manufacturer ones.
* **Express Signed / Certified Signed** – production signing; Certified Signed involves independent test-house testing and is required for restricted capabilities.

See [Signing and certification](23-signing-certification.md) for the full process and the post-2014 situation (the Symbian Signed service no longer exists).

## 2.6 Quick reference for agents

* Declaring a capability you don't need makes signing harder — declare the **minimum** in EXEs.
* Declaring too few in a **DLL** prevents high-capability processes from loading it — declare **generously** in DLLs.
* `KErrPermissionDenied` (-46) at runtime or "Unable to install" / "Required application access not granted" at install time almost always means a capability/certificate mismatch.
* Never write to `\sys\` or `\resource\` at runtime; store data in your private directory (`RFs::PrivatePath()`).

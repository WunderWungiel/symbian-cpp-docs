# 13. Libraries: Static LIBs, Shared DLLs, Polymorphic DLLs, Writable Static Data

> **Scope:** creating and using static libraries, statically-linked (shared) DLLs with exports, `.def` freezing, dynamically-loaded polymorphic DLLs via `RLibrary`, and the writable-static-data (WSD) problem.

## 13.1 Kinds of libraries

| Kind | MMP | UID2 | Linked | Shipped? |
|------|-----|------|--------|----------|
| **Static library** (`.lib`) | `TARGETTYPE lib` | — (`UID 0`) | Into the EXE/DLL at build time | No |
| **Static-interface (shared) DLL** | `TARGETTYPE dll` | `KSharedLibraryUid` = `0x1000008D` | At load time via an **import library** (`.lib` for EABI v1/WINSCW, `.dso` for EABI v2) | Yes, into `\sys\bin\` |
| **Polymorphic DLL** (plug-in) | `TARGETTYPE dll` (or `plugin`, `ani`, `fsy`, `prt`…) | Interface UID (e.g. `0x1000004A` = socket protocol module `.prt`) | At run time with `RLibrary::Load()`; one exported factory function | Yes |
| **ECom plug-in** | `TARGETTYPE plugin` | `0x10009D8D` | By the ECom framework | Yes — see [ECom](16-ecom.md) |

A DLL loaded statically is checked at load time: its UIDs must match the ones it had when the client was linked, and **capability rules** apply (the DLL must have at least the loading process's capabilities — see [Platform security](02-platform-security.md#22-capabilities)). A missing/incompatible DLL means the EXE doesn't start at all.

## 13.2 Static libraries

```text
// StaticLib.mmp
TARGET      StaticLib.lib
TARGETTYPE  lib
UID         0
SOURCEPATH  ..\src
SOURCE      StaticLib.cpp
USERINCLUDE ..\inc
SYSTEMINCLUDE \epoc32\include
```

* No `E32Main`, no UIDs, no capabilities (it becomes part of its user).
* Clients: copy/export the headers and add `STATICLIBRARY StaticLib.lib` to their MMP.
* Build the library **for every target** the client is built for (WINSCW, GCCE, ARMV5 — each in its own `\epoc32\release\<target>\<variant>\` folder).
* Third-party static libraries are distributed as one `.lib` per target plus headers.

## 13.3 Shared DLLs with a static interface

```text
// DynamicLib.mmp
TARGET      DynamicLib_0xE2EC91A6.dll
TARGETTYPE  dll
UID         0x1000008D 0xE2EC91A6
CAPABILITY  ALL -TCB            // generous, so any process can load it
SOURCEPATH  ..\src
SOURCE      DynamicLib.cpp
USERINCLUDE ..\inc
SYSTEMINCLUDE \epoc32\include
LIBRARY     euser.lib
```

Mark exported functions **`IMPORT_C` in the declaration** and **`EXPORT_C` in the definition**:

```cpp
// DynamicLib.h  — shipped to clients
#include <e32base.h>

const TInt KDynamicLibBufferLength = 15;
typedef TBuf<KDynamicLibBufferLength> TDynamicLibString;

class CDynamicLib : public CBase
    {
public:
    IMPORT_C static CDynamicLib* NewL();
    IMPORT_C static CDynamicLib* NewLC();
    IMPORT_C ~CDynamicLib();
    IMPORT_C TVersion Version() const;
    IMPORT_C void AddCharL(const TChar& aChar);
    IMPORT_C void RemoveLast();
    IMPORT_C TPtrC String() const;
    inline TInt Length() const { return iString->Length(); }   // inline: never exported
private:
    CDynamicLib();
    void ConstructL();
private:
    TDynamicLibString* iString;
    };
```

```cpp
// DynamicLib.cpp
#include "DynamicLib.h"

EXPORT_C CDynamicLib* CDynamicLib::NewLC()
    {
    CDynamicLib* self = new (ELeave) CDynamicLib;
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
    }

EXPORT_C CDynamicLib* CDynamicLib::NewL()
    {
    CDynamicLib* self = NewLC();
    CleanupStack::Pop(self);
    return self;
    }

CDynamicLib::CDynamicLib() {}
void CDynamicLib::ConstructL() { iString = new (ELeave) TDynamicLibString; }
EXPORT_C CDynamicLib::~CDynamicLib() { delete iString; }

EXPORT_C TVersion CDynamicLib::Version() const { return TVersion(1, 0, 1); }

EXPORT_C void CDynamicLib::AddCharL(const TChar& aChar)
    {
    if (iString->Length() >= KDynamicLibBufferLength)
        User::Leave(KErrOverflow);
    iString->Append(aChar);
    }

EXPORT_C void CDynamicLib::RemoveLast()
    {
    if (iString->Length() > 0)
        iString->SetLength(iString->Length() - 1);
    }

EXPORT_C TPtrC CDynamicLib::String() const { return *iString; }
```

* A non-exported member function declared in the public header but not marked `IMPORT_C` will cause link errors in clients (the linker looks for it in the client).
* Public data members need no markers. Virtual functions are reached through the vtable, but exporting them is still recommended for derivation.
* 9.x DLLs don't need an entry point; legacy templates contain `E32Dll()` for pre-9 SDKs.

### Producing the import library (freezing)
Exports are by **ordinal**; the `.def` file fixes the ordinals:

1. Build the DLL.
2. **Freeze**: `abld freeze` (Carbide: *Project → Freeze Exports*). Creates/updates `bwins\*.def` (WINSCW) and `eabi\*.def` (GCCE/ARMV5) — **separate files; keep both up to date**.
3. First time only: regenerate makefiles (clean / `bldmake bldfiles`).
4. Build again → the import library (`.lib`/`.dso`) is generated from the `.def`.

Every time you add an export: build → freeze → build. Never reorder or remove existing ordinals in a released DLL (binary compatibility). During early development `EXPORTUNFROZEN` in the MMP exports everything without a `.def`, but ordinals may change between builds (all clients must be rebuilt).

You can't create an import library for someone else's DLL without its `.def` — that's why libraries are distributed with `.lib`/`.dso` + headers. Import libraries are never shipped in a SIS.

### Using it
Client MMP: `LIBRARY DynamicLib_0xE2EC91A6.lib` (always write `.lib`; the tools use `.dso` when needed). Include the header, use the class normally:

```cpp
CDynamicLib* obj = CDynamicLib::NewLC();
obj->AddCharL('A');
TPtrC s = obj->String();
CleanupStack::PopAndDestroy(obj);
```

Tip: keep a private DLL in the **same project** as its only EXE (list the DLL's MMP first in `bld.inf`) to debug both together.

## 13.4 Polymorphic DLLs loaded at run time

Export exactly one factory function (ordinal 1) returning an object implementing a known abstract interface:

```cpp
// MyPluginInterface.h — the only header the host needs
class CMyPluginBase : public CBase
    {
public:
    virtual TVersion Version() const = 0;
    virtual void AddCharL(const TChar& aChar) = 0;
    virtual TPtrC String() const = 0;
    };

// In the plug-in DLL
EXPORT_C CMyPluginBase* NewPluginL()      // ordinal 1
    {
    return CMyPluginImpl::NewL();
    }
```

(Using a `CBase`-derived abstract base rather than an M class lets the host put the object on the cleanup stack and `delete` it through the base pointer.)

Host:

```cpp
typedef CMyPluginBase* (*TNewPluginL)();

RLibrary lib;
_LIT(KPluginName, "MyPlugin_0xE2EC91A7.dll");
const TUidType KUids(KDynamicLibraryUid, TUid::Uid(KMyInterfaceUid), KNullUid);
User::LeaveIfError(lib.Load(KPluginName, KUids));    // checks UID1/UID2; KNullUid = any
CleanupClosePushL(lib);

TLibraryFunction entry = lib.Lookup(1);              // by ordinal, starting at 1
if (!entry) User::Leave(KErrNotFound);
CMyPluginBase* plugin = reinterpret_cast<TNewPluginL>(entry)();
CleanupStack::PushL(plugin);
plugin->AddCharL('x');
CleanupStack::PopAndDestroy(plugin);                 // delete BEFORE closing the library!
CleanupStack::PopAndDestroy(&lib);
```

* **Delete all objects created by the DLL before `lib.Close()`** — their code (including destructors and vtables) is unloaded with the library → `KERN-EXEC 3` otherwise. (Push the library first, so it's popped last.)
* `RLibrary::Load(name [, path list] [, uids])`: e.g. `lib.Load(KName, _L("c:\\sys\\bin;e:\\sys\\bin"))`. `LoadRomLibrary()` for ROM DLLs. Other members: `Lookup(ordinal)` (returns `TLibraryFunction` = `TInt (*)()` — cast as needed), `FileName()`, `Type()` (UIDs), `GetRamSizes()`, `Duplicate()`.
* **No lookup by name** — ordinals only.
* Give your interface its own UID and use it as UID2 of every plug-in; check it when loading.
* For discoverable, registered plug-ins use **ECom** instead of hand-rolled `RLibrary` loading.

## 13.5 Writable static data (WSD) in DLLs

WSD = any non-const global or static variable, function-local `static`s, and **const objects with non-trivial constructors**:

```cpp
TBufC<20> fileName;                    // WSD
const TPoint KOrigin(0, 1);            // WSD (constructor runs at load)
static const TChar KExclamation('!');  // WSD
void F() { static TInt count; }        // WSD
_LIT(KName, "ok");                     // NOT WSD (TLitC is a POD constant)
const TInt KMax = 10;                  // NOT WSD
```

* EKA1 (pre-8.1b) DLLs **could not** have WSD at all. EKA2 supports it with **`EPOCALLOWDLLDATA`** in the MMP (on by default for WINSCW and some SDKs).
* It's **discouraged**: each process loading the DLL gets its own copy of the data section, costing at least one 4 KB page of RAM per process; ROM XIP DLLs handle it specially; in the **emulator a WSD DLL can be loaded by only one process at a time**.
* EXEs may have WSD freely.
* Alternatives:
  * move globals into classes (members of an object the client owns);
  * use a static library instead;
  * put all state into one object created at initialisation and store its pointer in **Thread-Local Storage**: `Dll::SetTls(ptr)`, `Dll::Tls()`, `Dll::FreeTls()` (one TLS slot per DLL per thread);
  * use a client–server design (state in the server).
* Symbian^3 added an emulator WSD helper (`emulator_wsd`) — not available on older SDKs.

## 13.6 Checklist

- [ ] Unique DLL file name (include UID or company prefix).
- [ ] UID2 `0x1000008D` for shared DLLs; interface UID for polymorphic DLLs.
- [ ] Capabilities ≥ those of every process that will load it (usually `ALL -TCB`).
- [ ] `IMPORT_C` in headers, `EXPORT_C` in sources; frozen `.def` for both `bwins` and `eabi`.
- [ ] No WSD (or `EPOCALLOWDLLDATA` consciously).
- [ ] DLL listed before its clients in `bld.inf`; included in the SIS at `!:\sys\bin\`.
- [ ] Objects from dynamically loaded DLLs destroyed before `RLibrary::Close()`.

## See also
* [SDK and build — freezing](05-sdk-and-build.md#55-freezing-dll-exports-def-files)
* [ECom plug-ins](16-ecom.md)

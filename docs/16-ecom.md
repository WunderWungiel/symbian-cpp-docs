# 16. ECom Plug-ins and Recognizers

> **Scope:** the ECom framework — defining an interface, implementing it in a plug-in DLL, registering it, listing and selecting implementations, custom resolvers; plus MIME-type recognizers.
> **When you need it:** extending system services (screensavers, FEPs, codecs, MTMs, protocols, recognizers, notifiers…) or giving your own app a plug-in architecture.

## 16.1 Concepts

* **ECom** ("EPOC Component Object Model") is a system server that **registers, discovers, selects and loads** plug-in implementations.
* A plug-in DLL (`TARGETTYPE plugin`, **UID2 = `0x10009D8D`**) contains one or more **implementations** (each with an **implementation UID**) of one or more **interfaces** (each with an **interface UID**).
* A **registration resource file** (`\resource\plugins\<dllname>.rsc`) describes what the DLL implements, so ECom doesn't need to load DLLs to find out.
* Clients ask ECom for an implementation of an interface UID; ECom's **resolver** picks one (default: match on `default_data`), loads the DLL, calls the factory function, and returns the object.
* Client API: **`REComSession`** (`ecom.h` in `\epoc32\include\ecom\`, library **`ecom.lib`**). Add `SYSTEMINCLUDE \epoc32\include\ecom` or `#include <ecom/ecom.h>`.

## 16.2 Defining the interface

Use an abstract **C-class** (so objects can go on the cleanup stack and be deleted normally) whose `NewL()` hides the ECom calls:

```cpp
// ArcInterface.h — shared by host and plug-ins
#include <e32base.h>
#include <ecom/ecom.h>

const TUid KArcInterfaceUid = { 0xEC2213E2 };

class CArcInterface : public CBase
    {
public:
    static CArcInterface* NewL();                        // first implementation found
    static CArcInterface* NewL(const TDesC8& aFormat);   // choose by default_data
    virtual ~CArcInterface();

    virtual TInt Compress(const TDesC& aFileName) = 0;
    virtual TInt Decompress(const TDesC& aFileName, TDes8& aResult) = 0;

private:
    TUid iDtor_ID_Key;      // set by ECom; identifies this instance on destruction
    };

inline CArcInterface* CArcInterface::NewL()
    {
    TAny* obj = REComSession::CreateImplementationL(KArcInterfaceUid,
                                                    _FOFF(CArcInterface, iDtor_ID_Key));
    return reinterpret_cast<CArcInterface*>(obj);
    }

inline CArcInterface* CArcInterface::NewL(const TDesC8& aFormat)
    {
    TEComResolverParams params;
    params.SetDataType(aFormat);
    // params.SetGenericMatch(ETrue);   // allow substring/wildcard match of default_data
    TAny* obj = REComSession::CreateImplementationL(KArcInterfaceUid,
                                                    _FOFF(CArcInterface, iDtor_ID_Key), params);
    return reinterpret_cast<CArcInterface*>(obj);
    }

inline CArcInterface::~CArcInterface()
    {
    REComSession::DestroyedImplementation(iDtor_ID_Key);   // tell ECom (allows DLL unload)
    }
```

Host usage:

```cpp
CArcInterface* zip = CArcInterface::NewL(_L8("ZIP"));
CleanupStack::PushL(zip);
User::LeaveIfError(zip->Compress(fileName));
CleanupStack::PopAndDestroy(zip);
...
REComSession::FinalClose();   // once, at program exit (e.g. AppUi destructor, or end of E32Main before __UHEAP_MARKEND)
```

* Without `FinalClose()`, ECom's per-thread session and loaded DLLs stay alive → **memory-leak panics** at exit in debug builds.
* Never call `FinalClose()` from the interface's destructor.
* An alternative uses `CreateImplementationL(interfaceUid, TUid& aDtorKey)` storing the key yourself.

### `REComSession` (mostly static)

| Function | Purpose |
|----------|---------|
| `CreateImplementationL(interfaceUid, dtorKeyOffset / TUid& key [, TEComResolverParams] [, resolverUid] [, initParams])` | Create the best match; or `CreateImplementationL(implementationUid, ...)` for a specific one |
| `ListImplementationsL(interfaceUid, RImplInfoPtrArray& [, params] [, resolverUid])` | Enumerate implementations (`CImplementationInformation`: `ImplementationUid()`, `DisplayName()`, `DataType()`, `OpaqueData()`, `Version()`, `Drive()`, `RomOnly()`…) |
| `DestroyedImplementation(key)` | Notify ECom that an instance is gone |
| `FinalClose()` | Release ECom resources for this thread |
| `OpenL()` + `NotifyOnChange(TRequestStatus&)` / `CancelNotifyOnChange()` / `Close()` | Be told when plug-ins are installed/removed |

```cpp
RImplInfoPtrArray infos;                          // RPointerArray<CImplementationInformation>
CleanupResetAndDestroyPushL(infos);
REComSession::ListImplementationsL(KArcInterfaceUid, infos);
for (TInt i = 0; i < infos.Count(); ++i)
    {
    const TDesC&  name   = infos[i]->DisplayName();
    const TDesC8& format = infos[i]->DataType();
    TUid          uid    = infos[i]->ImplementationUid();
    }
CleanupStack::PopAndDestroy(&infos);
```

## 16.3 Implementing a plug-in

```text
// ArcPlugin.mmp
TARGET          ArcPlugin_0xE18F0E7A.dll
TARGETTYPE      PLUGIN
UID             0x10009D8D 0xE18F0E7A
CAPABILITY      ALL -TCB          // must satisfy every process that may load it
SOURCEPATH      ..\src
SOURCE          ArcZip.cpp ArcRar.cpp Proxy.cpp
SOURCEPATH      ..\data
START RESOURCE  E18F0E7A.rss      // registration file: by convention named after the DLL UID
TARGET          ArcPlugin_0xE18F0E7A.rsc
END
USERINCLUDE     ..\inc
SYSTEMINCLUDE   \epoc32\include \epoc32\include\ecom
LIBRARY         euser.lib ecom.lib
```

```cpp
// ArcZip.h
class CArcZip : public CArcInterface
    {
public:
    static CArcZip* NewL();          // ECom needs only NewL (no NewLC)
    ~CArcZip();
    TInt Compress(const TDesC& aFileName);
    TInt Decompress(const TDesC& aFileName, TDes8& aResult);
private:
    CArcZip();
    void ConstructL();
    };

// ArcZip.cpp
CArcZip* CArcZip::NewL()
    {
    CArcZip* self = new (ELeave) CArcZip;
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }
```

The DLL exports **exactly one function** — the implementation proxy table:

```cpp
// Proxy.cpp
#include <ecom/implementationproxy.h>
#include "ArcZip.h"
#include "ArcRar.h"

const TImplementationProxy ImplementationTable[] =
    {
    IMPLEMENTATION_PROXY_ENTRY(0xE0003333, CArcZip::NewL),
    IMPLEMENTATION_PROXY_ENTRY(0xE0003334, CArcRar::NewL)
    };

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
    {
    aTableCount = sizeof(ImplementationTable) / sizeof(TImplementationProxy);
    return ImplementationTable;
    }
```

`TARGETTYPE PLUGIN` handles the `.def` for this single export automatically. If an `IMPLEMENTATION_PROXY_ENTRY` cast fails on newer compilers, cast the function: `(TProxyNewLPtr)CArcZip::NewL`.

## 16.4 The registration resource

```c
// E18F0E7A.rss
#include <ecom/registryinfo.rh>

RESOURCE REGISTRY_INFO theInfo
    {
    dll_uid = 0xE18F0E7A;                    // UID3 of the plug-in DLL
    interfaces =
        {
        INTERFACE_INFO
            {
            interface_uid = 0xEC2213E2;      // KArcInterfaceUid
            implementations =
                {
                IMPLEMENTATION_INFO
                    {
                    implementation_uid = 0xE0003333;
                    version_no   = 1;
                    display_name = "ZIP archive support";
                    default_data = "ZIP";    // matched by the default resolver
                    opaque_data  = "";       // extra data for custom resolvers / clients
                    },
                IMPLEMENTATION_INFO
                    {
                    implementation_uid = 0xE0003334;
                    version_no   = 1;
                    display_name = "RAR archive support";
                    default_data = "RAR";
                    opaque_data  = "";
                    }
                };
            }
        };
    }
```

* `BINARY_IMPLEMENTATION_INFO` is the same with byte arrays for `default_data`/`opaque_data`.
* Newer registry format (`RESOURCE_FORMAT_VERSION_2`/`3`) adds `rom_only` and extended interface info.
* The compiled `.rsc` must have the **same name as the DLL** and be installed to **`!:\resource\plugins\`**:

```text
"$(EPOCROOT)Epoc32\release\$(PLATFORM)\$(TARGET)\ArcPlugin_0xE18F0E7A.dll" - "!:\sys\bin\ArcPlugin_0xE18F0E7A.dll"
"$(EPOCROOT)Epoc32\data\z\resource\plugins\ArcPlugin_0xE18F0E7A.rsc"      - "!:\resource\plugins\ArcPlugin_0xE18F0E7A.rsc"
```

ECom scans `\resource\plugins\` on all drives at boot and whenever a drive or that folder changes (e.g. after installation).

## 16.5 Selecting implementations

* Default resolver: exact match of the `TEComResolverParams::SetDataType()` string against `default_data`. With `SetGenericMatch(ETrue)` (aka `SetWildcardMatch`), the data type may match as a substring / wildcard — so a "generic" implementation with `default_data = "ZIP||RAR||ARJ"` or similar can be chosen.
* The location of the implementation (which DLL, which drive) is hidden from the client.
* To pick a specific implementation, call `CreateImplementationL(implementationUid, ...)`.
* **Custom resolver:** implement the system interface `CResolver` (`resolver.h`, interface UID `KCustomResolverInterfaceUid` = `0x10009D90`) in an ECom plug-in — `IdentifyImplementationL()` and `ListAllL()` — and pass its implementation UID to `ListImplementationsL()`/`CreateImplementationL()`. Custom resolvers run inside the ECom server and need `ProtServ` (and must be ROM/trusted on many devices).

## 16.6 Common system plug-in interfaces (examples)

| Purpose | Interface / base | Notes |
|---------|------------------|-------|
| MIME recognizer | `CApaDataRecognizerType`, interface UID `0x101F7D87` | Below |
| Front-end processor (input method) | `CCoeFepPlugIn` | |
| Notifier | `MEikSrvNotifierBase2` | Global dialogs |
| Image decoders/encoders | `CImageDecoderPlugin` (ICL) | |
| Multimedia controllers/codecs | MMF controller plug-ins | |
| Messaging | MTM plug-ins | |
| Screensaver (S60) | Screensaver plug-in API | |
| Socket protocols | `.prt` polymorphic DLLs (not ECom) | UID2 `0x1000004A` |

## 16.7 Recognizers

The AppArc server uses **recognizers** to determine a file's MIME type, then launches the app registered with the highest priority for that type (`datatype_list` in its registration file — see [Applications](12-applications.md#124-registering-the-application-in-the-menu)).

* Pre-9.x: polymorphic `.mdl` DLLs. 9.x: **ECom plug-ins** implementing `CApaDataRecognizerType` (interface UID **`0x101F7D87`**). Must declare **`ProtServ`** (so a self-signed recognizer isn't possible).
* At recognition time each recognizer, in priority order, gets the file name and a buffer of the file's first bytes (default 256; `PreferredBufSize()` can ask for more) and reports a MIME type plus a confidence level (`ECertain`, `EProbable`, `EPossible`, `EUnlikely`, `ENotRecognized`).
* Using a recognizer to **autostart** an app (loaded at boot) was common pre-9; on 9.x use Startup List Management instead — recognizer-based autostart fails Symbian Signed.

```cpp
#include <apmrec.h>
#include <ecom/implementationproxy.h>

const TUid KMyRecognizerImplUid = { 0xE1234570 };
_LIT8(KMyMimeType, "application/x-myformat");
_LIT(KMyExtension, ".myf");

class CMyRecognizer : public CApaDataRecognizerType
    {
public:
    CMyRecognizer() : CApaDataRecognizerType(KMyRecognizerImplUid, CApaDataRecognizerType::ENormal)
        {
        iCountDataTypes = 1;
        }
    static CApaDataRecognizerType* CreateRecognizerL() { return new (ELeave) CMyRecognizer; }
    TUint PreferredBufSize() { return 16; }
    TDataType SupportedDataTypeL(TInt /*aIndex*/) const { return TDataType(KMyMimeType); }
private:
    void DoRecognizeL(const TDesC& aName, const TDesC8& aBuffer)
        {
        _LIT8(KMagic, "MYF1");
        if (aBuffer.Left(KMagic().Length()) == KMagic)
            {
            iDataType = TDataType(KMyMimeType);
            iConfidence = ECertain;
            }
        else if (aName.Right(KMyExtension().Length()).CompareF(KMyExtension) == 0)
            {
            iDataType = TDataType(KMyMimeType);
            iConfidence = EProbable;
            }
        // else: leave iConfidence = ENotRecognized
        }
    };

const TImplementationProxy ImplementationTable[] =
    {
    IMPLEMENTATION_PROXY_ENTRY(0xE1234570, CMyRecognizer::CreateRecognizerL)
    };

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
    {
    aTableCount = sizeof(ImplementationTable) / sizeof(TImplementationProxy);
    return ImplementationTable;
    }
```

MMP: `TARGETTYPE PLUGIN`, `UID 0x10009D8D <dll uid>`, `CAPABILITY ProtServ` (plus whatever else), `LIBRARY euser.lib apmime.lib`. Registration resource with `interface_uid = 0x101F7D87`.

Programmatic recognition: `RApaLsSession::RecognizeData(name, buffer, TDataRecognitionResult&)` or `AppForDocument()`.

## 16.8 Checklist

- [ ] Interface: abstract C-class with `TUid iDtor_ID_Key`, `NewL` via `CreateImplementationL`, destructor calls `DestroyedImplementation`.
- [ ] Host calls `REComSession::FinalClose()` once at exit.
- [ ] Plug-in: `TARGETTYPE PLUGIN`, UID2 `0x10009D8D`, single export `ImplementationGroupProxy`.
- [ ] Registration `.rsc` named like the DLL, installed in `\resource\plugins\`; `dll_uid` = DLL UID3.
- [ ] Capabilities sufficient for every host process (system servers load many plug-ins!).
- [ ] Unique interface and implementation UIDs.

## See also
* [DLLs](13-dlls-libraries.md)
* [Applications — MIME registration](12-applications.md)

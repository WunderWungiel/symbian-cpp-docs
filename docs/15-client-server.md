# 15. The Client–Server Framework

> **Scope:** writing a Symbian server (`CServer2`, `CSession2`, `CPolicyServer`), its client-side session class (`RSessionBase`), messages (`RMessage2`, `TIpcArgs`), starting servers on demand, transient-server shutdown, sub-sessions, asynchronous requests, and security.
> **Where you meet it:** every system service (file server `RFs`, socket server `RSocketServ`, window server `RWsSession`…) is a server. You write your own for background services, shared resources, or privilege separation.

## 15.1 Concepts

```
 Client thread/process                Kernel                     Server process
 ┌──────────────────┐   message   ┌──────────────┐  ServiceL()  ┌──────────────────┐
 │ RMySession        │ ──────────► │ session obj  │ ───────────► │ CMySession        │
 │  (RSessionBase)   │ ◄────────── │ message slots│ ◄─────────── │  (CSession2)      │
 └──────────────────┘   Complete()└──────────────┘              │ CMyServer(CServer2)│
                                                                └──────────────────┘
```

* The server is an **active object** (`CServer2`) in its own thread — almost always its own process (so it has its own capabilities and SID).
* A **session** is a kernel object connecting one client to the server: represented by an `RSessionBase`-derived class on the client and a `CSession2`-derived object on the server.
* Each request is a **message**: a function code (`TInt`) + up to **4 arguments** (integers, pointers to client descriptors, handles). The server completes it with a `TInt` result.
* Requests can be **synchronous** (client blocks) or **asynchronous** (client passes a `TRequestStatus`; the server is then an async service provider). Only one synchronous request per session at a time; many async ones.
* Client and server can't touch each other's memory; descriptor data is copied by the kernel (`RMessage2::ReadL`/`WriteL`).
* **Persistent** servers start at boot and live forever; **transient** servers are started by the first client and exit (after a delay) when the last session closes.
* Servers usually ship a **client DLL** exporting the R-class API, so clients never see function codes (e.g. `RFs` in `efsrv.dll`, link with `efsrv.lib`).
* Classes with a `2` suffix (`CServer2`, `CSession2`, `RMessage2`) are the 9.x Platform-Security-aware versions. `CServer`/`CSession`/`RMessage` are pre-9 and obsolete.

## 15.2 Shared definitions

```cpp
// MySimpleServerCommon.h — shared by client DLL and server
_LIT(KMyServerName, "MySimpleServer");            // or "!MySimpleServer" — protected name, needs ProtServ
_LIT(KMyServerExe,  "MySimpleServer_0xE0102040.exe");
const TUid KMyServerUid3 = { 0xE0102040 };
const TInt KMyServerMajor = 1;
const TInt KMyServerMinor = 0;
const TInt KMyServerBuild = 0;

enum TMyServerFunction
    {
    EMyNotify,          // no arguments
    EMySendInt,         // arg0: TInt
    EMySendDes,         // arg0: const TDesC*
    EMySendStruct,      // arg0: TPckg<TMyData>
    EMyRequestInt,      // arg0: TPckg<TInt> (out)
    EMyRequestDes,      // arg0: TDes* (out)
    EMyRequestStruct,   // arg0: TPckg<TMyData> (out)
    EMyNotifyChange,    // async; completes when something changes
    EMyCancelNotifyChange
    };

struct TMyData
    {
    TInt      iDataInt;
    TBuf<100> iDataBuf;
    };
```

## 15.3 The server

```cpp
class CMyServer : public CServer2
    {
public:
    static CMyServer* NewLC();
    void AddSession();
    void RemoveSession();
private:
    CMyServer() : CServer2(EPriorityStandard, ESharableSessions) {}
    void ConstructL();
    CSession2* NewSessionL(const TVersion& aVersion, const RMessage2& aMessage) const;
private:
    TInt      iSessionCount;
    CShutdown iShutdown;          // see 15.6
    };

CMyServer* CMyServer::NewLC()
    {
    CMyServer* self = new (ELeave) CMyServer;
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
    }

void CMyServer::ConstructL()
    {
    StartL(KMyServerName);        // register the name; leaves KErrAlreadyExists if running
    iShutdown.ConstructL();
    iShutdown.Start();            // exit if no client connects soon
    }

CSession2* CMyServer::NewSessionL(const TVersion& aVersion, const RMessage2& /*aMessage*/) const
    {
    if (!User::QueryVersionSupported(TVersion(KMyServerMajor, KMyServerMinor, KMyServerBuild), aVersion))
        User::Leave(KErrNotSupported);
    return new (ELeave) CMySession;
    }
```

`TServerType` (second constructor argument): `EUnsharableSessions` (default — session usable only by the thread that created it), `ESharableSessions` (any thread of the client process), `EGlobalSharableSessions` (any process).

`CServer2` API: `Start(name)` / `StartL(name)`, `ReStart()`, `Server()` (the `RServer2` handle), `Message()` (current message), `NewSessionL()` (pure virtual), `iSessionIter` (iterate sessions).

### Server process entry point

```cpp
static void RunServerL()
    {
    User::LeaveIfError(RThread::RenameMe(KMyServerName));   // optional; helps debugging
    CActiveScheduler* s = new (ELeave) CActiveScheduler;
    CleanupStack::PushL(s);
    CActiveScheduler::Install(s);

    CMyServer::NewLC();                 // starts listening

    RProcess::Rendezvous(KErrNone);     // tell the launching client we're ready
    CActiveScheduler::Start();          // until CActiveScheduler::Stop()

    CleanupStack::PopAndDestroy(2, s);  // server, scheduler
    }

GLDEF_C TInt E32Main()
    {
    __UHEAP_MARK;
    CTrapCleanup* cleanup = CTrapCleanup::New();
    TInt err = KErrNoMemory;
    if (cleanup)
        {
        TRAP(err, RunServerL());
        delete cleanup;
        }
    __UHEAP_MARKEND;
    return err;
    }
```

If `StartL` leaves with `KErrAlreadyExists` (another instance won the race), the launching client's `Rendezvous` completes with that code — the client treats it as success and connects to the existing instance.

## 15.4 The server-side session

```cpp
class CMySession : public CSession2
    {
public:
    void CreateL();                                         // called after construction
    ~CMySession();
private:
    void ServiceL(const RMessage2& aMessage);
    void ServiceError(const RMessage2& aMessage, TInt aError);
    CMyServer& Server() { return *static_cast<CMyServer*>(const_cast<CServer2*>(CSession2::Server())); }
    void ReadDesL(const RMessage2& aMessage);
    void WriteStructL(const RMessage2& aMessage);
private:
    TInt     iInt;
    HBufC*   iDes;
    TMyData  iData;
    RMessage2 iNotifyMsg;      // pending async request (null if none)
    };

void CMySession::CreateL()
    {
    Server().AddSession();
    }

CMySession::~CMySession()
    {
    if (!iNotifyMsg.IsNull())
        iNotifyMsg.Complete(KErrCancel);
    delete iDes;
    Server().RemoveSession();
    }

void CMySession::ServiceL(const RMessage2& aMessage)
    {
    switch (aMessage.Function())
        {
        case EMyNotify:
            aMessage.Complete(KErrNone);
            break;
        case EMySendInt:
            iInt = aMessage.Int0();
            aMessage.Complete(KErrNone);
            break;
        case EMySendDes:
            ReadDesL(aMessage);
            break;
        case EMySendStruct:
            {
            TPckg<TMyData> pkg(iData);
            aMessage.ReadL(0, pkg);
            aMessage.Complete(KErrNone);
            }
            break;
        case EMyRequestInt:
            {
            TPckgC<TInt> pkg(iInt);
            aMessage.WriteL(0, pkg);
            aMessage.Complete(KErrNone);
            }
            break;
        case EMyRequestDes:
            if (!iDes) { aMessage.Complete(KErrNotReady); break; }
            aMessage.WriteL(0, *iDes);      // leaves KErrOverflow if the client buffer is too small
            aMessage.Complete(KErrNone);
            break;
        case EMyRequestStruct:
            WriteStructL(aMessage);
            break;
        case EMyNotifyChange:               // asynchronous: keep the message, complete later
            if (!iNotifyMsg.IsNull()) { aMessage.Complete(KErrInUse); break; }
            iNotifyMsg = aMessage;
            break;
        case EMyCancelNotifyChange:
            if (!iNotifyMsg.IsNull()) iNotifyMsg.Complete(KErrCancel);
            aMessage.Complete(KErrNone);
            break;
        default:
            _LIT(KPanicCat, "MyServer");
            aMessage.Panic(KPanicCat, KErrNotSupported);   // standard: panic clients sending bad codes
            break;
        }
    }

void CMySession::ReadDesL(const RMessage2& aMessage)
    {
    TInt len = aMessage.GetDesLengthL(0);
    if (!iDes || iDes->Des().MaxLength() < len)
        {
        delete iDes;
        iDes = NULL;
        iDes = HBufC::NewL(len);
        }
    TPtr ptr(iDes->Des());
    aMessage.ReadL(0, ptr);
    aMessage.Complete(KErrNone);
    }

void CMySession::WriteStructL(const RMessage2& aMessage)
    {
    TPckgC<TMyData> pkg(iData);
    aMessage.WriteL(0, pkg);
    aMessage.Complete(KErrNone);
    }

void CMySession::ServiceError(const RMessage2& aMessage, TInt aError)
    {
    aMessage.Complete(aError);   // default behaviour; ServiceL leaves end up here
    }
```

### `RMessage2` reference

| Member | Purpose |
|--------|---------|
| `Function()` | Function code |
| `Int0()`…`Int3()`, `Ptr0()`…`Ptr3()` | Raw arguments |
| `GetDesLength[L](i)`, `GetDesMaxLength[L](i)` | Length/max length of a client descriptor argument |
| `Read[L](i, TDes8/16&, offset)` | Copy from client descriptor |
| `Write[L](i, const TDesC8/16&, offset)` | Copy into client descriptor (sets its length) |
| `Complete(TInt)` / `Complete(RHandleBase)` | Finish the request (or return a handle); the message becomes null |
| `IsNull()` | Already completed? |
| `Session()`, `ClientStatus()` (NULL for sync) | |
| `Client[L](RThread&)`, `SecureId()`, `VendorId()`, `HasCapability(cap[, diag])`, `CheckPolicy()` | Identify/authorise the client |
| `Panic(cat, reason)`, `Kill()`, `Terminate()` | Punish a misbehaving client |

**Always complete every message exactly once** (forgetting leaves the client hung; completing twice panics). For async requests, store the `RMessage2` (a copy is fine — it's a handle) and complete it later; complete pending messages with `KErrCancel` on cancel and in the session destructor.

ServiceL runs inside the server's `RunL()`: one message at a time. Long operations should be delegated to other active objects (pass the stored `RMessage2`) so the server stays responsive.

## 15.5 The client-side session class and starting the server on demand

```cpp
class RMySession : public RSessionBase
    {
public:
    IMPORT_C TInt Connect();
    IMPORT_C TInt SendInt(TInt aValue);
    IMPORT_C TInt SendDes(const TDesC& aDes);
    IMPORT_C TInt SendStruct(const TMyData& aData);
    IMPORT_C TInt RequestInt(TInt& aValue);
    IMPORT_C TInt RequestDes(TDes& aDes);
    IMPORT_C TInt RequestStruct(TMyData& aData);
    IMPORT_C void NotifyChange(TRequestStatus& aStatus);
    IMPORT_C void CancelNotifyChange();
    };

static TInt StartServer()
    {
    RProcess server;
    TInt err = server.Create(KMyServerExe, KNullDesC, TUidType(KNullUid, KNullUid, KMyServerUid3));
    if (err != KErrNone)
        return err;
    TRequestStatus status;
    server.Rendezvous(status);
    if (status != KRequestPending)
        server.Kill(0);                 // abort start-up
    else
        server.Resume();
    User::WaitForRequest(status);       // wait for RProcess::Rendezvous() in the server (or its death)
    err = (server.ExitType() == EExitPanic) ? KErrGeneral : status.Int();
    server.Close();
    return err;
    }

EXPORT_C TInt RMySession::Connect()
    {
    const TVersion version(KMyServerMajor, KMyServerMinor, KMyServerBuild);
    const TInt KAsyncMessageSlots = 2;
    TInt retry = 2;
    for (;;)
        {
        TInt err = CreateSession(KMyServerName, version, KAsyncMessageSlots);
        if (err != KErrNotFound && err != KErrServerTerminated)
            return err;
        if (--retry == 0)
            return err;
        err = StartServer();
        if (err != KErrNone && err != KErrAlreadyExists)
            return err;
        }
    }

EXPORT_C TInt RMySession::SendInt(TInt aValue)
    {
    return SendReceive(EMySendInt, TIpcArgs(aValue));
    }

EXPORT_C TInt RMySession::SendDes(const TDesC& aDes)
    {
    return SendReceive(EMySendDes, TIpcArgs(&aDes));      // pass descriptors by POINTER
    }

EXPORT_C TInt RMySession::SendStruct(const TMyData& aData)
    {
    TPckgC<TMyData> pkg(aData);
    return SendReceive(EMySendStruct, TIpcArgs(&pkg));
    }

EXPORT_C TInt RMySession::RequestInt(TInt& aValue)
    {
    TPckg<TInt> pkg(aValue);
    return SendReceive(EMyRequestInt, TIpcArgs(&pkg));
    }

EXPORT_C TInt RMySession::RequestDes(TDes& aDes)
    {
    return SendReceive(EMyRequestDes, TIpcArgs(&aDes));
    }

EXPORT_C TInt RMySession::RequestStruct(TMyData& aData)
    {
    TPckg<TMyData> pkg(aData);
    return SendReceive(EMyRequestStruct, TIpcArgs(&pkg));
    }

EXPORT_C void RMySession::NotifyChange(TRequestStatus& aStatus)
    {
    SendReceive(EMyNotifyChange, TIpcArgs(), aStatus);    // async overload
    }

EXPORT_C void RMySession::CancelNotifyChange()
    {
    SendReceive(EMyCancelNotifyChange);
    }
```

Usage:

```cpp
RMySession session;
User::LeaveIfError(session.Connect());
CleanupClosePushL(session);
User::LeaveIfError(session.SendInt(42));
TInt v;
User::LeaveIfError(session.RequestInt(v));
CleanupStack::PopAndDestroy(&session);   // Close() ends the session; server's CMySession is destroyed
```

`RSessionBase` (protected API): `CreateSession(name|RServer2, TVersion [, asyncSlots] [, TIpcSessionType] [, const TSecurityPolicy*] [, TRequestStatus*])`, `Open(...)` (from a passed handle), `Send(fn, args)` (fire-and-forget), `SendReceive(fn, args)` (sync) / `SendReceive(fn, args, status)` (async), `ShareAuto()` / `ShareProtected()` (allow other threads/processes to use the session), `Close()`.

**Data passing rules**
* `TIpcArgs` holds 0–4 arguments: `TInt`, `TAny*`, `const TDesC*`/`TDes*` (8 or 16 bit), `RHandleBase` handles, or `TIpcArgs::ENothing`.
* Pass descriptors **by pointer**; structures via **`TPckg`/`TPckgC`/`TPckgBuf`**.
* For **async** calls (and `Send()`), every descriptor/package must **stay alive until completion** — make them members of the client object, not locals.
* Message slots: the server has a pool; `CreateSession(..., aAsyncMessageSlots)` reserves slots for this session (fails with `KErrServerBusy` if unavailable). Default (`-1`) uses the global pool and may fail at send time with `KErrServerBusy`.
* Every IPC round trip costs two context switches: batch data into fewer, larger requests where performance matters.

## 15.6 Transient server shutdown

`CServer2` doesn't count sessions — do it yourself and stop the scheduler after a grace period (pattern from *Symbian OS Explained*):

```cpp
const TInt KShutdownDelay = 2000000;   // 2 s

class CShutdown : public CTimer
    {
public:
    CShutdown() : CTimer(-1) { CActiveScheduler::Add(this); }
    void ConstructL() { CTimer::ConstructL(); }
    void Start() { After(KShutdownDelay); }
private:
    void RunL() { CActiveScheduler::Stop(); }
    };

void CMyServer::AddSession()    { ++iSessionCount; iShutdown.Cancel(); }
void CMyServer::RemoveSession() { if (--iSessionCount == 0) iShutdown.Start(); }
```

The delay lets a client that reconnects quickly avoid a costly restart. Use the "start on `KErrNotFound`" logic in `Connect()` even for persistent servers, to recover if one died.

## 15.7 Sub-sessions

A client can hold several lightweight **sub-sessions** over one session (e.g. many `RFile`s over one `RFs`). Client class derives from `RSubSessionBase` (`CreateSubSession(session, fn, args)`, `CloseSubSession(fn)`, `SendReceive()`); the server keeps per-subsession objects (often in a `CObjectIx`/`CObjectCon` index) and maps the handle in each message to the right object. Closing the session closes all its sub-sessions.

## 15.8 Security

* **Name squatting:** clients find servers by name, so a malicious process could register your name first. Names beginning with **`!`** can only be registered by processes with **`ProtServ`**. Alternatively, pass a `TSecurityPolicy` to `CreateSession()` so the client verifies the server's SID/capabilities.
* **Client checks:** use `aMessage.SecureId()`, `HasCapability()`, `CheckPolicy()` — or derive from **`CPolicyServer`**, which checks a static policy table per function range before `ServiceL()`:

```cpp
const TUint KRangeCount = 3;
const TInt KRanges[KRangeCount] = { 0, EMyRequestInt, EMyCancelNotifyChange + 1 };
const TUint8 KElementsIndex[KRangeCount] =
    {
    CPolicyServer::EAlwaysPass,        // 0 .. EMyRequestInt-1
    0,                                 // EMyRequestInt .. EMyCancelNotifyChange: element 0
    CPolicyServer::ENotSupported       // anything above
    };
const CPolicyServer::TPolicyElement KElements[] =
    {
    { _INIT_SECURITY_POLICY_C1(ECapabilityReadUserData), CPolicyServer::EFailClient }
    };
const CPolicyServer::TPolicy KMyPolicy =
    {
    CPolicyServer::EAlwaysPass,        // connect policy
    KRangeCount, KRanges, KElementsIndex, KElements
    };

class CMyServer : public CPolicyServer
    {
    CMyServer() : CPolicyServer(EPriorityStandard, KMyPolicy, ESharableSessions) {}
    ...
    };
```

* A server holding capabilities is a privilege boundary: validate every argument, never trust client lengths/indexes, and only expose what each client is allowed to do.

## 15.9 Checklist

- [ ] Server EXE: cleanup stack, scheduler, `StartL(name)`, `RProcess::Rendezvous(KErrNone)`, `CActiveScheduler::Start()`.
- [ ] `NewSessionL` checks the version; sessions count themselves in/out.
- [ ] Every message completed exactly once; bad function codes panic the client.
- [ ] Pending async messages completed on cancel and in the session destructor.
- [ ] Client `Connect()` starts the server on `KErrNotFound`/`KErrServerTerminated`.
- [ ] Descriptors passed by pointer; structs via packages; async buffers are members.
- [ ] Protected server name (`!`) or a server security policy if spoofing matters.
- [ ] Build order in `bld.inf`: common/client DLL, server EXE, then client apps.

## See also
* [Processes and threads](14-processes-threads-ipc.md) — `RProcess::Rendezvous`
* [Active objects](11-active-objects.md)

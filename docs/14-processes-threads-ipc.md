# 14. Processes, Threads, Synchronisation and IPC

> **Scope:** `RProcess`, `RThread`, finding kernel objects, exit monitoring and rendezvous, mutexes, condition variables, semaphores, critical sections, shared chunks, message queues, Publish & Subscribe.
> **Rule of thumb:** prefer active objects over threads; prefer client–server, Publish & Subscribe or message queues over shared memory.

## 14.1 Processes and threads basics

* A **process** is a container and a unit of **memory protection and trust** (capabilities, SID). A **thread** is the unit of **scheduling**.
* Launching an EXE creates a process with one **main thread**; when the main thread exits, the process ends.
* Process priority only serves to compute the absolute priorities of its threads.
* Limits (S60 3rd era): roughly 128 threads per process, fewer if each has its own heap; on ARMv5 a process can have up to 16 memory **chunks** (stack+heap+code+WSD take 3–4; each thread with its own heap one more).
* Kernel-object handles (`RProcess`, `RThread`, `RMutex`…) take an optional `TOwnerType`: `EOwnerProcess` (default — usable from all threads of the process) or `EOwnerThread` (only from the thread that opened it).
* `Close()` on any of these closes **only your handle**, not the object.

## 14.2 `RProcess`

A default-constructed `RProcess` refers to the **current process** (`KCurrentProcessHandle`).

```cpp
RProcess me;
TSecureId sid = me.SecureId();
TFileName exe = me.FileName();

// Open another process by name or by ID
RProcess other;
User::LeaveIfError(other.Open(_L("MyProcessName")));
other.Close();

// Find by pattern
TFindProcess finder(_L("MyProc*"));
TFullName name;
while (finder.Next(name) == KErrNone)
    {
    RProcess p;
    if (p.Open(finder) == KErrNone) { /* … */ p.Close(); }
    }

// Launch an EXE
RProcess proc;
User::LeaveIfError(proc.Create(_L("MyServer_0xE1234568.exe"), KNullDesC));   // name [, command line]
proc.Resume();          // processes are created suspended
proc.Close();
```

| Member | Purpose |
|--------|---------|
| `Create(exe, cmdline [, TUidType] [, owner])` | Start an EXE (`.exe` assumed; searched in `\sys\bin`) |
| `Open(name / TProcessId / TFindProcess)` | Attach to an existing process |
| `Resume()` | Let a newly created process run |
| `FileName()`, `Id()`, `Type()`, `SecureId()`, `VendorId()` | Info |
| `HasCapability(ECapabilityX)` | Capability check |
| `RenameMe(name)` (static) | Rename the current process |
| `Priority()`, `SetPriority()` | `EPriorityLow`, `EPriorityBackground`, `EPriorityForeground`, `EPriorityHigh` (higher values are system-only → panic) |
| `Kill(reason)`, `Terminate(reason)`, `Panic(cat, reason)` | End **another** process — requires **`PowerMgmt`** |
| `ExitType()` (`EExitKill`, `EExitTerminate`, `EExitPanic`, `EExitPending`), `ExitCategory()`, `ExitReason()` | Why it ended |
| `Logon(TRequestStatus&)`, `LogonCancel()` | Async notification on exit; status = exit reason |
| `Rendezvous(TRequestStatus&)`, `RendezvousCancel()` | Async: completes when the target calls `RProcess::Rendezvous(code)` (or exits) |
| `RProcess::Rendezvous(TInt)` (static) | "I'm ready" signal from the running process |
| `GetMemoryInfo()`, `CommandLine()`/`User::CommandLine()` | Misc. |

Watching for exit:

```cpp
TRequestStatus status;
proc.Logon(status);
User::WaitForRequest(status);         // or use an active object
switch (proc.ExitType())
    {
    case EExitKill:      break;
    case EExitTerminate: break;
    case EExitPanic:
        {
        TBuf<32> info(proc.ExitCategory());   // e.g. "KERN-EXEC"
        info.Append(' ');
        info.AppendNum(proc.ExitReason());    // e.g. 3
        }
        break;
    default: break;
    }
```

**Rendezvous** is how a client starts a server and waits until it's ready (see [Client–server](15-client-server.md#155-the-client-side-session-class-and-starting-the-server-on-demand)):

```cpp
TRequestStatus rs;
proc.Rendezvous(rs);
if (rs != KRequestPending) proc.Kill(0);     // abort
else proc.Resume();
User::WaitForRequest(rs);                    // completes when the child calls RProcess::Rendezvous(KErrNone)
TInt err = (proc.ExitType() == EExitPanic) ? KErrGeneral : rs.Int();
proc.Close();
```

`RUndertaker` lets you observe deaths of **all** threads in the system (used for crash loggers).

Launching **applications** (rather than EXEs) is usually done through AppArc: `RApaLsSession::StartApp(CApaCommandLine&)` or `StartDocument()`; to bring an app to the foreground use `TApaTaskList`/`TApaTask` (`FindApp(uid)`, `BringToForeground()`, `EndTask()`).

## 14.3 `RThread`

```cpp
static TInt ThreadFunction(TAny* aParams)
    {
    CTrapCleanup* cleanup = CTrapCleanup::New();        // every thread needs its own cleanup stack
    if (!cleanup) return KErrNoMemory;
    TRAPD(err, DoThreadWorkL(aParams));                 // and its own active scheduler, if it uses AOs
    delete cleanup;
    return err;
    }

void StartWorkerL()
    {
    RThread thread;
    const TInt KStackSize = 0x2000;                     // 8 KB is typical
    User::LeaveIfError(thread.Create(
        _L("MyWorker"),                 // unique name (else KErrAlreadyExists)
        ThreadFunction,                 // TInt (*)(TAny*)
        KStackSize,
        NULL,                           // NULL: share the creating thread's heap; or use the (min,max) overload for an own heap
        this));                         // argument passed to ThreadFunction
    thread.SetPriority(EPriorityLess);  // optional
    thread.Resume();                    // threads are created suspended
    thread.Close();                     // our handle only
    }
```

* Overloads: `Create(name, func, stack, minHeap, maxHeap, arg)` gives the thread **its own heap**; `Create(name, func, stack, RAllocator* heap, arg)` shares one (`NULL` = current thread's heap).
* GUI code belongs in the main thread; worker threads normally don't touch UI controls.
* `Suspend()`, `Resume()`, `Kill()`, `Terminate()`, `Panic()`, `Logon()`, `Rendezvous()`, `ExitType()`, `ExitReason()`… mirror `RProcess`.
* `RThread::RequestComplete(TRequestStatus*&, TInt)` completes a request **in another thread** (the `RThread` must be opened on the thread that issued the request). `RequestSignal()` signals the thread semaphore (same process only).
* `RequestCount()` returns the thread semaphore count (negative = waiting).
* `HandleCount()`, `StackInfo()` for diagnostics.
* A dying thread takes time to clean up; re-creating a thread with the same name immediately may fail with `KErrAlreadyExists`. Use unique names (e.g. append a counter) or wait on `Logon()`.
* Thread priorities: `EPriorityMuchLess`, `EPriorityLess`, `EPriorityNormal`, `EPriorityMore`, `EPriorityMuchMore`, `EPriorityRealTime`; absolute ones (`EPriorityAbsoluteBackground`…) exist too.
* `User::After()` / `User::AfterHighRes()` sleep the current thread.

## 14.4 Synchronisation primitives

All are kernel objects, usable across threads (and processes, if global/named). Global ones are found with `TFindMutex`, `TFindSemaphore`, etc. (same pattern as `TFindProcess`).

### Mutex — `RMutex`

```cpp
_LIT(KMutexName, "MyTestMutex");
RMutex mutex;
if (mutex.OpenGlobal(KMutexName) != KErrNone)
    User::LeaveIfError(mutex.CreateGlobal(KMutexName));     // or CreateLocal() for this process only
CleanupClosePushL(mutex);

mutex.Wait();       // acquire (recursive: same thread may Wait again)
// ... access shared resource ...
mutex.Signal();     // release (once per Wait)

CleanupStack::PopAndDestroy(&mutex);
```

`IsHeld()` tells whether the current thread holds it. Waiting threads are released in priority order. The kernel object is destroyed when the last handle closes.

### Condition variable — `RCondVar` (9.x)
Used with a mutex to wait until a condition becomes true:

```cpp
// Consumer
mutex.Wait();
while (!QueueHasAtLeast(n))
    condvar.Wait(mutex);    // atomically releases the mutex and blocks; re-acquires before returning
ReadAllFromQueue();
mutex.Signal();

// Producer
mutex.Wait();
WriteOneToQueue();
condvar.Signal();           // wake one waiter (Broadcast() wakes all)
mutex.Signal();
```

`CreateGlobal()/CreateLocal()/OpenGlobal()`, `TimedWait(mutex, microseconds)`. There's no `TFind` class — you need the exact name. Always re-check the condition in a loop.

### Semaphore — `RSemaphore`
Counting semaphore: up to *n* threads may pass.

```cpp
_LIT(KSemName, "MyTestSemaphore");
RSemaphore sem;
if (sem.OpenGlobal(KSemName) != KErrNone)
    User::LeaveIfError(sem.CreateGlobal(KSemName, 2));      // initial count 2
CleanupClosePushL(sem);
User::LeaveIfError(sem.Wait(10000000));    // timed wait (10 s) → KErrTimedOut
// ... use one of two slots ...
sem.Signal();                              // Signal(n) adds n
CleanupStack::PopAndDestroy(&sem);
```

### Critical section — `RCriticalSection`
A fast, **unnamed, process-local** mutual-exclusion object (derived from `RSemaphore`; avoids kernel calls when uncontended). `CreateLocal()`, `Wait()`, `Signal()`, `IsBlocked()`. Share the same object (or copies of its handle) between threads of one process. `RFastLock` is a similar lightweight lock.

## 14.5 Inter-thread vs inter-process communication

Threads of one process share an address space — they can share objects via pointers (pass one as the thread function argument) as long as access is synchronised. Between processes you need IPC:

| Mechanism | Best for |
|-----------|----------|
| **Client–server** | Request/response services, access control, async notifications — the standard Symbian pattern. See [Client–server](15-client-server.md). |
| **Publish & Subscribe** (`RProperty`) | Broadcasting small state values to many subscribers (system state, settings) |
| **Message queues** (`RMsgQueue`) | One-way, fixed-size messages between a few threads |
| **Shared chunks** (`RChunk`) | Bulk/high-throughput data (drivers, multimedia); you handle synchronisation |
| `RPipe` (9.3+) | Byte-stream pipes (not in public SDKs at first) |
| Files, Central Repository | Persistent shared data |

### Shared memory — `RChunk`

```cpp
_LIT(KChunkName, "MySampleChunk");
const TInt KSize = 0x4000;
RChunk chunk;
if (chunk.OpenGlobal(KChunkName, EFalse) != KErrNone)          // EFalse = read/write
    User::LeaveIfError(chunk.CreateGlobal(KChunkName, KSize, KSize * 3));
CleanupClosePushL(chunk);

RMemWriteStream ws(chunk.Base(), chunk.Size());                // see Streams
CleanupClosePushL(ws);
ws.WriteInt32L(42);
CleanupStack::PopAndDestroy(2, &chunk);
```

Size is page-aligned; memory is freed when the last handle closes (no destructors run for objects placed in it). `CreateLocal()` for unnamed chunks shared by handle.

### Message queues — `RMsgQueue<T>` (`e32msgqueue.h`)
Fixed-size messages (**max 256 bytes**) in a bounded queue:

```cpp
struct TMyMessage
    {
    TInt       iValue;
    TBufC<10>  iText;
    };

_LIT(KQueueName, "MyMsgQueue");
RMsgQueue<TMyMessage> queue;
if (queue.OpenGlobal(KQueueName) != KErrNone)
    User::LeaveIfError(queue.CreateGlobal(KQueueName, 5));    // 5 slots
CleanupClosePushL(queue);

TMyMessage msg;
msg.iValue = 1;
TInt err = queue.Send(msg);         // KErrOverflow if full; SendBlocking() waits
err = queue.Receive(msg);           // KErrUnderflow if empty; ReceiveBlocking() waits
// Async: NotifyDataAvailable(TRequestStatus&) / CancelDataAvailable(),
//        NotifySpaceAvailable(TRequestStatus&) / CancelSpaceAvailable()
CleanupStack::PopAndDestroy(&queue);
```

No peek: each message is consumed by one reader, so broadcasting to many clients is awkward — use P&S for that.

### Publish & Subscribe — `RProperty` (`e32property.h`)
Kernel-side **properties** identified by **(category UID, key)**, holding an integer, a byte array (≤ 512 bytes, `KMaxPropertySize`) or a large byte array (≤ 65535 bytes, `KMaxLargePropertySize`). They survive until deleted or reboot (not across reboots — use Central Repository for persistence).

**Defining** (publisher):

```cpp
const TUid KMyCategory = { 0xE1234567 };     // normally your own SID — no capability needed
enum TMyKeys { EMyCounterKey = 0, EMyTextKey, EMyLargeKey };

_LIT_SECURITY_POLICY_PASS(KReadPolicy);                          // anyone may read
_LIT_SECURITY_POLICY_S0(KWritePolicy, 0xE1234567);               // only this SID may write
TInt err = RProperty::Define(KMyCategory, EMyCounterKey, RProperty::EInt, KReadPolicy, KWritePolicy);
if (err != KErrNone && err != KErrAlreadyExists) User::Leave(err);
RProperty::Define(KMyCategory, EMyTextKey,  RProperty::EText, KReadPolicy, KWritePolicy);
RProperty::Define(KMyCategory, EMyLargeKey, RProperty::ELargeText, KReadPolicy, KWritePolicy, 1000);  // pre-allocate

RProperty::Set(KMyCategory, EMyCounterKey, 5);   // static, slower; or Attach() then Set()
```

* Category = your **SID**: no capability needed. A different category must be below `KUidSecurityThresholdCategoryValue` (`0x10273357`) and requires **`WriteDeviceData`** (commonly `KUidSystemCategory` `0x101F75B6`). Otherwise `KErrPermissionDenied`.
* Security policies (`TSecurityPolicy` / `_LIT_SECURITY_POLICY_*` macros: `PASS`, `FAIL`, `C1..C7` capabilities, `S0..S3` SID + caps, `V0..V3` VID + caps) control read and write access.
* Types: `EInt`, `EByteArray`/`EText`, `ELargeByteArray`/`ELargeText`.

**Subscribing** (any number of readers) — typically an active object:

```cpp
class CPropertyWatcher : public CActive
    {
public:
    static CPropertyWatcher* NewL(TUid aCat, TUint aKey);
    ~CPropertyWatcher() { Cancel(); iProperty.Close(); }
private:
    CPropertyWatcher() : CActive(EPriorityStandard) {}
    void ConstructL(TUid aCat, TUint aKey)
        {
        User::LeaveIfError(iProperty.Attach(aCat, aKey));   // OK even if not yet defined
        CActiveScheduler::Add(this);
        Subscribe();
        }
    void Subscribe() { iProperty.Subscribe(iStatus); SetActive(); }
    void RunL()
        {
        TInt status = iStatus.Int();
        Subscribe();                              // re-subscribe FIRST, then read — don't miss updates
        if (status == KErrNotFound) return;       // property deleted
        TInt value;
        if (iProperty.Get(value) == KErrNone) HandleValue(value);
        }
    void DoCancel() { iProperty.Cancel(); }
    void HandleValue(TInt aValue);
    RProperty iProperty;
    };
```

* `Subscribe()` completes on **every** `Set()`, even with the same value. If values change rapidly you see only the latest one.
* `Get()`/`Set()` overloads for `TInt`, `TDes8`/`TDesC8`, `TDes16`/`TDesC16`; static variants take category+key without `Attach()`.
* `RProperty::Delete(cat, key)` (owner only) completes subscribers with `KErrNotFound`.
* Many system states are published this way, e.g. `KPSUidStartup`, `KUidSystemCategory` + `KUidPhonePwr`, battery/charger and call state keys (headers like `e32property.h`, `sacls.h`, `ctsydomainpskeys.h`, `hwrmpowerstatesdkpskeys.h`).

## See also
* [Active objects](11-active-objects.md)
* [Client–server](15-client-server.md)
* [Streams](19-streams-stores.md) — `RMemReadStream`/`RMemWriteStream`

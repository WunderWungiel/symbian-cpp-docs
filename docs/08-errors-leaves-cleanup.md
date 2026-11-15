# 8. Errors, Leaves, Panics, Memory and the Cleanup Stack

> **Scope:** error codes, leaves and traps, panics, assertions, heap/stack, leak detection, the cleanup stack.
> **The three rules agents most often break:** (1) never leave in a constructor or destructor; (2) anything allocated and not yet owned by a member must be on the cleanup stack across any call that can leave; (3) every `PushL` must be balanced by `Pop`/`PopAndDestroy` before the enclosing TRAP ends.

## 8.1 Error codes

Non-critical failures are reported by **returning a `TInt` error code**. System codes are in `e32err.h`; all are **negative**, success is **`KErrNone` (0)**.

| Code | Value | | Code | Value |
|------|------:|-|------|------:|
| `KErrNone` | 0 | | `KErrDied` | -13 |
| `KErrNotFound` | -1 | | `KErrInUse` | -14 |
| `KErrGeneral` | -2 | | `KErrServerTerminated` | -15 |
| `KErrCancel` | -3 | | `KErrServerBusy` | -16 |
| `KErrNoMemory` | -4 | | `KErrCompletion` | -17 |
| `KErrNotSupported` | -5 | | `KErrNotReady` | -18 |
| `KErrArgument` | -6 | | `KErrUnknown` | -19 |
| `KErrTotalLossOfPrecision` | -7 | | `KErrCorrupt` | -20 |
| `KErrBadHandle` | -8 | | `KErrAccessDenied` | -21 |
| `KErrOverflow` | -9 | | `KErrLocked` | -22 |
| `KErrUnderflow` | -10 | | `KErrWrite` | -23 |
| `KErrAlreadyExists` | -11 | | `KErrDisMounted` | -24 |
| `KErrPathNotFound` | -12 | | `KErrEof` | -25 |
| `KErrDiskFull` | -26 | | `KErrBadName` | -28 |
| `KErrTimedOut` | -33 | | `KErrCouldNotConnect` | -34 |
| `KErrAbort` | -39 | | `KErrPermissionDenied` | -46 |
| `KErrBadDescriptor` | -38 | | `KErrHardwareNotAvailable` | -44 |

(Subsystem-specific codes exist too, e.g. sockets/HTTP use their own ranges.)

```cpp
TInt CPeople::GetAge(TInt aIdx) const
    {
    if (aIdx < 0)                    return KErrArgument;
    if (aIdx >= iAgeArray.Count())   return KErrOverflow;
    return iAgeArray[aIdx];
    }
```

## 8.2 Leaves — Symbian exceptions

Critical failures **leave**: the function stops immediately and control unwinds to the nearest **trap**, carrying a `TInt` error code. A leave is like `longjmp`; on 9.x it's implemented on top of C++ `throw` (an `XLeaveException`).

How a function can leave:

```cpp
User::Leave(KErrArgument);           // unconditional
User::LeaveNoMemory();               // == User::Leave(KErrNoMemory)
User::LeaveIfError(err);             // leaves if err < 0, otherwise returns err
ptr = User::LeaveIfNull(ptr);        // leaves KErrNoMemory if NULL
CFoo* p = new (ELeave) CFoo;         // leaves on allocation failure; never returns NULL
SomethingL();                        // calling any L function
```

```cpp
TInt CPeople::GetAgeL(TInt aIdx) const
    {
    return User::LeaveIfError(GetAge(aIdx));
    }
```

Many system APIs offer both variants (e.g. `RArray::Append()` returns an error, `RArray::AppendL()` leaves). A function should generally **either** return an error **or** leave, not both.

**Leaves are for errors, not for normal control flow.**

## 8.3 Traps

Catch a leave with one of three macros:

```cpp
TInt err;
TRAP(err, DoWorkL());                // err declared by you
TRAPD(err2, DoWorkL());              // declares TInt err2 for you
TRAP_IGNORE(DoWorkL());              // catch and discard
if (err != KErrNone) { /* handle */ }
```

* A function containing a TRAP around everything it does that can leave is itself **non-leaving** (no `L` suffix).
* Traps are **relatively expensive** (they set up a cleanup-stack mark and a try/catch). Minimise their number: group leaving calls into one `L` function and trap that once.

```cpp
// Wasteful, and semantically different: each Append is attempted independently
TRAP_IGNORE(iAges.AppendL(25));
TRAP_IGNORE(iAges.AppendL(33));

// Better: one trap; the sequence stops at the first failure
void CPeople::FillL() { iAges.AppendL(25); iAges.AppendL(33); }
...
TRAPD(err, FillL());
```

* Every leave **must** eventually be trapped. In GUI apps and servers the framework traps around your callbacks (e.g. `HandleCommandL`, `RunL` via `RunError`) and shows/handles the error. An untrapped leave in a thread you created causes a panic (`USER-EXEC 3`/`E32USER-CBase 66` if there's no trap harness).
* **Don't mix C++ exceptions and leaves.** On 9.x, `throw`/`try`/`catch` exist, but TRAPs can't catch C++ exceptions and `catch` can't safely handle leaves (it bypasses the cleanup stack). Use one mechanism per binary — leaves, except when porting standard C++ code (e.g. Open C/C++ libraries), where you convert at the boundary. Nested exceptions are not supported on target: throwing while another is being handled calls `abort()`.

## 8.4 Panics

A **panic** terminates the **thread** immediately — no unwinding, no cleanup. If it's the main thread, the whole process dies. If the process is marked *system-critical*, the phone reboots. Panics signal **programming errors** (invalid arguments, misuse of an API, broken invariants). A shipped application must never panic.

A panic has a **category** (string ≤ 16 characters) and a **reason** (integer). Look both up in the SDK help ("System panic reference").

```cpp
// MyApp.pan
enum TMyAppPanic
    {
    EMyAppNullEngine = 1,
    EMyAppBadIndex
    };

inline void Panic(TMyAppPanic aReason)
    {
    _LIT(KCategory, "MyApp");
    User::Panic(KCategory, aReason);
    }
```

### Common system panics

| Panic | Meaning |
|-------|---------|
| `KERN-EXEC 0` | Bad handle (using a closed/invalid R object) |
| `KERN-EXEC 3` | **Access violation** — NULL/dangling pointer, misalignment, stack overflow |
| `KERN-EXEC 15` | A timer request was issued while one was outstanding |
| `E32USER-CBase 40` | Active object destroyed while its request is still outstanding (destructor didn't call `Cancel()`) |
| `E32USER-CBase 42` | `CActive::SetActive()` called on an AO that is already active |
| `E32USER-CBase 46` | **Stray signal** — a request completed for an AO that wasn't active (forgot `SetActive()`, AO not added to the scheduler, or the request used a `TRequestStatus` other than `iStatus`) |
| `E32USER-CBase 47` | `RunL()` left and `RunError()` returned the error → default `CActiveScheduler::Error()` panics |
| `E32USER-CBase 66` | Cleanup stack used with no TRAP harness (no mark) |
| `E32USER-CBase 69` | No cleanup stack installed in this thread (`CTrapCleanup` missing) |
| `E32USER-CBase 90` | `Pop(ptr)`/`PopAndDestroy(ptr)` — the top item isn't `ptr` |
| other `E32USER-CBase` | Cleanup stack imbalance — more pops than pushes, or items left pushed when a TRAP exits |
| `USER 9`, `USER 10` | Descriptor position/length out of range (`Left`, `Right`, `Mid`, `Delete`, `Replace`…) |
| `USER 11` | **Descriptor overflow** — writing more than `MaxLength()` |
| `USER 42` | Freeing a heap cell with a bad address (e.g. via an M-class pointer) |
| `USER 44` | Double `delete` (heap corruption) |
| `USER 130` | `RArray`/`RPointerArray` index out of bounds |
| `USER 133` | `TFixedArray` index out of bounds (UDEB) |
| `ALLOC: <addr>` | **Memory leak** detected at exit (UDEB, by `__UHEAP_MARKEND` / app exit) |
| `ViewSrv 11` | App didn't respond to the view server in time (long synchronous work in the UI thread) |
| `EIKCOCTL 70`, `EIKON-LISTBOX` | UI control misuse |
| `CONE 14`/`CONE 8` | Control environment errors (e.g. control on stack twice) |
| `Kill`/`Terminate` | Not a panic: the process was killed |

### Seeing panic details on a device
By default S60 just says "App closed". To get category and code, create an empty file **`C:\resource\ErrRd`** (no extension). The built-in file manager can't write there — install a tiny SIS containing it (widely available as "ErrRd.sis"), or use a third-party file manager with `AllFiles`.

In the emulator, uncheck **Tools → Preferences → C++ Debug → Just In Time** to see the panic dialog instead of the emulator closing, or per program:

```cpp
#ifdef __WINS__
    User::SetJustInTime(EFalse);
#endif
```

## 8.5 Assertions

```cpp
__ASSERT_DEBUG(aAge >= 0, Panic(EMyAppBadAge));        // UDEB only — removed in UREL
__ASSERT_ALWAYS(aAge <= KMaxAge, Panic(EMyAppBadAge));  // every build
ASSERT(iEngine);                                        // UDEB; panics "USER 0"
```

* Never put required work inside `__ASSERT_DEBUG` — it disappears in release builds. Compute first, then assert on the result.
* The second argument can be any expression, but it's meant to stop execution (normally a panic).
* Use `__ASSERT_ALWAYS` sparingly — it panics in shipped code.

## 8.6 Memory: heap and stack

* Each **process** has its own address space; processes can't touch each other's memory (use IPC). Each **thread** has its own **stack** and shares the process **heap** by default (a thread can be given its own heap).
* **Heap:** sizes from `EPOCHEAPSIZE` (default 4 KB min, 1 MB max). Grows in 4 KB pages. Cells are word-aligned, ≥ 12 bytes including a header. Use for large or variable-size data.
* **Stack:** fixed size, default 8 KB (`0x2000`) for the main thread, max 80 KB; GUI apps need ≥ 20 KB (`0x5000`). Overflow panics (on the device — the emulator grows the stack). Keep big objects (e.g. `TBuf<256>`, `TFileName` = 512 bytes!) off the stack.

```cpp
TInt* p = new TInt;              // returns NULL on failure — must check
TInt* q = new (ELeave) TInt;     // leaves on failure — never NULL
delete q;                        // delete NULL is safe; delete twice = USER 44
TInt* arr = new (ELeave) TInt[10];
delete[] arr;

TAny* raw = User::AllocL(1024);  // also User::Alloc (NULL on failure), User::AllocLC
User::Free(raw);
```

### Zero-leak policy and leak detection
Symbian's policy is **no leaks, ever**. In debug builds an application that exits with unfreed heap cells triggers an **`ALLOC` panic** showing the leaked cell address (and fails certification).

```cpp
__UHEAP_MARK;          // start checking (UDEB only)
DoTestL();
__UHEAP_MARKEND;       // panics ALLOC if any cell allocated since MARK is still alive
// __UHEAP_MARKENDC(n) — expect exactly n remaining cells
// __UHEAP_FAILNEXT(n), __UHEAP_SETFAIL(RHeap::EDeterministic, n), __UHEAP_RESET
//   — simulate allocation failure to test your leave paths (OOM testing)
```

Typical use: around `MainL()` in console tests. The emulator's `Ctrl+Alt+Shift+P` tool and `__UHEAP_FAILNEXT` loops are how you verify that every leave path cleans up.

## 8.7 The cleanup stack

Leaves bypass normal returns, so local heap objects would leak:

```cpp
void LeakyL()
    {
    CFoo* foo = CFoo::NewL();
    BarL();                 // if this leaves, foo leaks
    delete foo;
    }
```

The **cleanup stack** records objects that must be destroyed if a leave occurs. Each TRAP pushes a **mark**; on leave, everything pushed since the nearest mark is destroyed (LIFO), then execution resumes after the TRAP.

```cpp
void SafeL()
    {
    CFoo* foo = CFoo::NewL();
    CleanupStack::PushL(foo);
    BarL();                               // if this leaves, foo is deleted
    CleanupStack::PopAndDestroy(foo);     // pop + delete in one step
    }
```

### Setup
* GUI apps and servers: the framework creates it and provides a trap — just use it.
* Console programs and **every thread you create**: create one yourself, **before** the first TRAP:

```cpp
GLDEF_C TInt E32Main()
    {
    __UHEAP_MARK;
    CTrapCleanup* cleanup = CTrapCleanup::New();   // NULL if out of memory
    TInt err = KErrNoMemory;
    if (cleanup)
        {
        TRAP(err, MainL());
        delete cleanup;
        }
    __UHEAP_MARKEND;
    return err;
    }
```

Only one `CTrapCleanup` per thread. Calling `CleanupStack` functions with no cleanup stack → `E32USER-CBase 69`; with no trap mark → `E32USER-CBase 66`.

### API

| Call | Effect |
|------|--------|
| `CleanupStack::PushL(CBase* p)` | On leave: `delete p` (virtual destructor runs) |
| `CleanupStack::PushL(TAny* p)` | On leave: `User::Free(p)` — **no destructor** (for T-objects / raw memory) |
| `CleanupStack::PushL(TCleanupItem(func, p))` | On leave: `func(p)` — custom cleanup |
| `CleanupClosePushL(r)` | On leave: `r.Close()` (R classes, `RArray`, `RBuf`) |
| `CleanupReleasePushL(r)` | On leave: `r.Release()` (e.g. ECom/MMF interfaces) |
| `CleanupDeletePushL(p)` | On leave: `delete p` with the real type's destructor — for non-CBase classes (e.g. M-class pointers with virtual destructors) |
| `CleanupArrayDeletePushL(p)` | On leave: `delete[] p` |
| `CleanupResetAndDestroyPushL(a)` | On leave: `a.ResetAndDestroy()` (e.g. `RPointerArray`, ECom `RImplInfoPtrArray`) |
| `CleanupStack::Pop()` / `Pop(n)` | Remove top item(s) without destroying |
| `CleanupStack::Pop(p)` / `Pop(n, pLast)` | Same, and (UDEB) check that the last item popped is `p` (`E32USER-CBase 90` otherwise) |
| `CleanupStack::PopAndDestroy()` / `(n)` / `(p)` / `(n, pLast)` | Pop and destroy |
| `CleanupStack::Check(p)` | Panic if `p` isn't on top (all builds) |

**`PushL` itself can leave** (allocating a slot) — but it's still safe: the stack always keeps a free slot, so the object is recorded before the leave can happen.

### Rules
1. **Push immediately** after allocation, before calling anything that can leave.
2. **Never push member variables.** Members are destroyed by the owner's destructor; push the owner.
3. **Don't delete an object while it's still on the cleanup stack** — pop it first (or use `PopAndDestroy`).
4. **Pop before the TRAP that pushed it ends**; anything pushed inside a TRAP must be popped inside it (`E32USER-CBase 71`). So an `LC` function can't be the direct body of a TRAP:

```cpp
TInt* FuncL()                     // returns ownership, stack balanced
    {
    TInt* x = new (ELeave) TInt;
    CleanupStack::PushL(x);
    SomethingL(x);
    CleanupStack::Pop(x);
    return x;
    }

void DoStartL()
    {
    TInt* obj = NULL;
    TRAPD(err, obj = FuncL());    // OK: FuncL leaves the stack balanced
    if (err == KErrNone)
        {
        CleanupStack::PushL(obj);
        UseL(obj);
        CleanupStack::PopAndDestroy(obj);
        }
    }
```

5. Objects already on the stack can be used inside nested TRAPs without popping.
6. **Transfer of ownership** — when you hand a pushed object to an owner that may leave while accepting it, pop *after* the hand-over succeeds:

```cpp
CItem* item = CItem::NewLC();
iItems.AppendL(item);             // may leave — item still protected
CleanupStack::Pop(item);          // now owned by iItems
```

7. Use the pointer form `Pop(p)` / `PopAndDestroy(p)` everywhere — it catches imbalance bugs in debug builds.
8. For a `TCleanupItem`, the function signature is `void Func(TAny* aPtr)`:

```cpp
static void CleanupHeapInt(TAny* aPtr) { delete static_cast<TInt*>(aPtr); }
...
TInt* i = new (ELeave) TInt;
CleanupStack::PushL(TCleanupItem(CleanupHeapInt, i));
```

## 8.8 L-classes (EUserHL)

From Symbian^3 (and as a back-ported library for S60 5th Edition/3rd FP2), the **EUser High Level library** (`euserhl.lib`, headers `<euserhl.h>`, `<estring.h>`) adds RAII helpers prefixed **L**, which use the cleanup stack internally:

* `LString` — a self-growing, self-freeing string (wraps `RBuf`).
* `LCleanedupPtr<T>`, `LCleanedupHandle<T>`, `LCleanedupRef<T>`, `LCleanedupArray<T>` — local ownership that is automatically cleaned up on leave **and** on scope exit.
* `LManagedPtr<T>`, `LManagedHandle<T>` … — the same for class members.
* `CONSTRUCTORS_MAY_LEAVE` macro — lets a class's constructor leave safely when its members are `LManaged*`.

```cpp
LCleanedupPtr<CFoo> foo(CFoo::NewL());   // no PushL/PopAndDestroy needed
foo->DoL();
LString s;
s.FormatL(_L("%d items"), count);        // grows as needed
```

They are convenient but uncommon in classic code; many projects target 3rd Edition and can't use them.

## 8.9 Leave-safety checklist for agents

- [ ] Every function that can leave ends in `L` (or `LC`, `LD`); every function without `L` traps internally.
- [ ] Constructors and destructors contain no leaving calls.
- [ ] Every local heap pointer or open R handle is on the cleanup stack across leaving calls.
- [ ] Members are not pushed; destructors free everything owned, coping with partial construction.
- [ ] Each `PushL` is matched by `Pop`/`PopAndDestroy` (pointer form) on every path.
- [ ] No `LC` function is the direct body of a `TRAP`.
- [ ] `new (ELeave)` (not plain `new`) unless NULL is explicitly handled.
- [ ] Threads and console EXEs create a `CTrapCleanup` and a top-level `TRAP`.
- [ ] Debug builds tested with `__UHEAP_MARK`/`MARKEND` and OOM simulation.

## See also
* [Fundamentals and class categories](07-fundamentals.md)
* [Active objects](11-active-objects.md) — `RunL`/`RunError` and leaves

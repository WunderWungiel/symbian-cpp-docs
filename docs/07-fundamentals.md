# 7. Symbian C++ Fundamentals: Types, Naming, Class Categories

> **Scope:** the "dialect" of C++ used on Symbian — fundamental types, the naming convention, the four class categories (T, C, R, M), static classes, and two-phase construction.
> **Why it's different:** Symbian C++ predates standard C++ (no reliable exceptions, templates or STL on the compilers of the 1990s) and targeted devices with a few MB of RAM. It therefore defines its own types, its own exception mechanism (*leaves*), its own string classes (*descriptors*) and its own concurrency model (*active objects*). **Even experienced C++ programmers find it hard to learn — follow the idioms exactly.**

## 7.1 Fundamental types

Defined in `e32def.h` / `e32const.h`. Always use these instead of built-in C++ types — they are guaranteed compiler-independent.

| Symbian | C++ equivalent | Notes |
|---------|----------------|-------|
| `TInt8` / `TUint8` | `signed char` / `unsigned char` | |
| `TInt16` / `TUint16` | `short` / `unsigned short` | |
| `TInt32` / `TUint32` | `long` / `unsigned long` | |
| `TInt` / `TUint` | `int` / `unsigned int` | At least 32 bits. Use for general integers. |
| `TInt64` / `TUint64` | `long long` / `unsigned long long` | |
| `TText8` / `TText16` | `unsigned char` / `unsigned short` | Character types |
| `TText` | `TText16` | Default character type (Unicode builds) |
| `TReal32` | `float` | |
| `TReal64`, `TReal` | `double` | Prefer `TReal`. No FPU on many devices — floating point is slow. |
| `TBool` | `int` | Values `ETrue` (1) / `EFalse` (0) |
| `TAny` | `void` | **Only** as `TAny*` (generic pointer). Use plain `void` for "no return value". |

```cpp
void Foo(TAny* aParam);   // right
TAny Foo();               // wrong — use void
```

**`TBool` pitfall:** it's an `int`, so it can hold values other than 0/1. Never compare with `ETrue`:

```cpp
if (b) { ... }            // right
if (b == ETrue) { ... }   // wrong — fails if b == 2
```

Other common basic types: `TUid` (wrapper for a UID; `TUid::Uid(0x...)`, `KNullUid`), `TUidType` (three UIDs), `TVersion`, `TTime`/`TDateTime` (see [Time](17-time-timers.md)), `TPoint`, `TSize`, `TRect`, `TRgb`, `TChar`.

Useful constants and macros (`e32def.h`, `e32const.h`): `NULL`, `KMaxTInt`, `KMinTInt`, `KMaxTUint`, `KMaxFileName` (0x100 = 256), `KMaxName` (0x80), `FOREVER` (`for(;;)`), `ASSERT`, `__ASSERT_DEBUG`, `__ASSERT_ALWAYS`, `_LIT`, `_L`, `_FOFF`, `IMPORT_C` / `EXPORT_C`, `inline`, `const_cast`/`STATIC_CAST`/`REINTERPRET_CAST` macros, `Min()`/`Max()`/`Abs()` (in `e32std.h`/`e32cmn.h`).

## 7.2 Naming convention

The names tell you how to create, use and destroy an object. **Follow them strictly** — tools, reviewers and other Symbian developers rely on them.

| Element | Rule | Examples |
|---------|------|----------|
| General | `UpperCamelCase`, optional one-letter prefix | `CMyEngine` |
| Local (automatic) variables | `lowerCamelCase`, **no prefix** | `count`, `fileName` |
| Member variables | prefix `i` | `iCount`, `iEngine` |
| Function arguments | prefix `a` | `aIndex`, `aFileName` |
| Constants | prefix `K` | `KMaxFileName`, `KErrNone`, `KMyLiteral` |
| Enumerations (the type) | prefix `T` | `TTimeFormat` |
| Enum values | prefix `E` | `ETime12`, `EMyCmdOpen` |
| Macros | `UPPER_CASE` with underscores | `FOREVER`, `STATIC_CAST`, `_LIT` |
| Debug/platform macros | leading `__` | `__ASSERT_DEBUG`, `__WINS__`, `__UHEAP_MARK` |
| Structs | prefix `T` (legacy: `S`) | `TInfo`, `SLocaleLocaleSettings` |
| Classes | prefix by category: `T`, `C`, `R`, `M`; static classes have **no** prefix | `TBuf`, `CActive`, `RFile`, `MCoeControlObserver`, `User`, `CleanupStack`, `Mem` |

Occasionally system headers violate these rules (constants with `E`, enums with `K`) when an API evolved and source compatibility was preserved.

### Function name suffixes

| Suffix | Meaning | Example |
|--------|---------|---------|
| `L` | **May leave** (throw a Symbian exception). Callers must be leave-safe. | `AppendL()`, `NewL()` |
| `C` | **Leaves something on the cleanup stack** when it returns | `NewLC()`, `AllocLC()`, `LoadLC()` |
| `D` | **Destroys the object** it's called on (`delete this`) | `ExecuteLD()`, `RunLD()` |

Combinations: `LC`, `LD`. A function without `L` must **never** leave. Functions that can panic don't need a suffix (anything can panic).

```cpp
void CMyDialog::RunLD()
    {
    CleanupStack::PushL(this);
    RunL();
    CleanupStack::PopAndDestroy(this);   // delete this
    }
```

### Other conventions
* Getters omit `Get`: `Age()`, not `GetAge()`. A `Get` prefix means the function **returns data through a reference argument**: `GetName(TDes& aName)`.
* Setters: `SetAge(TInt aAge)`.
* Trailing-`L` names for leaving virtuals like `RunL()`, `HandleCommandL()`.
* Header guard style: `#ifndef __MYCLASS_H__`.
* Indentation in Symbian/Nokia code puts braces at the same indentation as the body ("Whitesmiths"); not required, but you'll see it everywhere.

## 7.3 Class categories

### T classes — "types" (value types)
* Fundamental types, enums, structs and simple value classes (`TBuf`, `TPoint`, `TTime`, `TPckg`, `TLex`).
* May live on the stack or the heap (large ones — e.g. `TBuf<256>`, `TLex` — preferably as members of a C class to save stack).
* **Own no resources** that need releasing: members are only other T types, references or pointers to things the T class does not own.
* **No destructor** (technically allowed on 9.x, but the cleanup stack won't call it and earlier OS versions didn't call it for stack objects).
* Members are **not** zero-initialised (except when the T object is a member of a C class) → initialise in a constructor.
* Copyable by value; pass large ones by `const&`.

```cpp
class TMyAge
    {
public:
    TMyAge() : iAge(0), iNext(NULL) {}
    void SetAgeL(TInt aAge) { if (aAge < 0) User::Leave(KErrArgument); iAge = aAge; }
    inline TInt Age() const { return iAge; }
public:
    TMyAge* iNext;   // not owned
private:
    TInt iAge;
    };
```

### C classes — "classes" (heap objects that own things)
* Derive (directly or indirectly) from **`CBase`** (`e32base.h`), which provides:
  * `operator new` that **zero-fills** memory → all members start as 0/NULL;
  * a **virtual destructor** → `CleanupStack::PushL(CBase*)` and `delete` through base pointers work correctly;
  * **private copy constructor and assignment** → C objects are not copied; pass by pointer/reference.
* **Always allocated on the heap** (`new (ELeave)`), never on the stack.
* May own anything: other C objects (usually by pointer), R handles, buffers.
* Use **two-phase construction** (below) when initialisation can leave.
* In the destructor, `delete` owned pointers — **no need to set them to NULL** afterwards (except when the pointer may be deleted again elsewhere, e.g. in a "reset" method: `delete iX; iX = NULL;`).

### R classes — "resources" (handles)
* Handles to resources owned elsewhere — usually by a server or the kernel: `RFs` (file server session), `RFile`, `RSocket`, `RThread`, `RMutex`, `RTimer`, `RArray`, `RBuf`, `RWindow`.
* No common base class. Most hold a single integer handle; **zero = not open**.
* Usually on the stack or as members of a C class; trivial constructor; usually no destructor; copy constructor/assignment typically private → pass by reference.
* **Explicit open and close**: `Open()`, `Connect()`, `Create()`, `Duplicate()` … then `Close()` (or `Reset()`, `Destroy()`, `Free()`, `Release()`). Going out of scope does **not** close the handle — forgetting `Close()` leaks the resource (and can lock files or keep server sessions open).
* Push onto the cleanup stack with `CleanupClosePushL()`, `CleanupReleasePushL()` or `CleanupDeletePushL()`:

```cpp
void CFileReader::ReadFileL()
    {
    RFile file;
    User::LeaveIfError(file.Open(iFs, KMyFile, EFileRead | EFileShareReadersOnly));
    CleanupClosePushL(file);
    // ... leaving reads ...
    CleanupStack::PopAndDestroy(&file);   // calls file.Close()
    // `file` can be opened again here
    }
```

  Members (`iFs`) are **not** pushed — they are closed in the owner's destructor, and the owner itself is what goes on the cleanup stack.

### M classes — "mixins" (interfaces)
* Abstract interface classes, mostly **observer/callback** interfaces (`MCoeControlObserver`, `MEikMenuObserver`, `MMdaAudioPlayerCallback`).
* **No data members, no constructors, no overloaded operators.** Usually only pure virtual functions; may provide default implementations when all implementers share behaviour. May declare a virtual destructor (but an M pointer normally doesn't own its object).
* Never instantiated.
* **Multiple inheritance rule:** inherit from **at most one** concrete base class (CBase-derived) and any number of M classes, and **list the C base first**:

```cpp
class CListener : public CBase, public MClockObserver, public MTimeoutObserver
```

  If an M class comes first, `CleanupStack::PushL()` picks the `TAny*` overload (the object pointer is offset), and the cleanup stack frees the wrong address → `USER 42` / `E32USER-CBase 90` panics.
* You can't delete an object through an M pointer (the address isn't the object start) unless the M class has a virtual destructor.

Callback example:

```cpp
class MClockObserver
    {
public:
    virtual void WakeUpL(TAny* aParam) = 0;
    };

class CClock : public CActive
    {
public:
    CClock(MClockObserver& aObserver) : CActive(EPriorityStandard), iObserver(aObserver) {}
    TInt Ring(TAny* aParam)
        {
        TRAPD(err, iObserver.WakeUpL(aParam));
        return err;
        }
private:
    MClockObserver& iObserver;   // not owned
    // ... RunL/DoCancel omitted
    };

class CListener : public CBase, public MClockObserver
    {
private:
    void WakeUpL(TAny* aParam);  // from MClockObserver
    };
```

### Static classes
Utility classes with only static functions; no prefix; can't be instantiated: `User` (leave, panic, heap, timing), `Mem` (memory copy/fill/compare), `CleanupStack`, `Math`, `BaflUtils`, `EikonEnv`-style helpers, `AknUtils`.

### L classes (Symbian^3 / 9.5+)
Newer code may use `L`-classes (e.g. `LString`, `LCleanedupPtr`, `LManagedHandle` from the *EUserHL* library, `euserhl.lib`) — RAII-style self-managing types that integrate with the cleanup stack. See [Errors and cleanup](08-errors-leaves-cleanup.md#88-l-classes-euserhl).

## 7.4 Two-phase construction

**Rule: constructors must never leave.** If `new (ELeave) CFoo` succeeds and then the constructor leaves, the memory isn't yet on the cleanup stack and leaks. Destructors must never leave either.

So construction is split:

1. **Phase 1:** C++ constructor — only non-leaving initialisation (member init list).
2. **Phase 2:** `ConstructL()` — everything that can leave (allocations, opening handles) — called **after** the object is on the cleanup stack.

Both are private/protected; clients use static factory functions `NewL()` / `NewLC()`:

```cpp
// MyEngine.h
class CMyEngine : public CBase
    {
public:
    static CMyEngine* NewL(TInt aSize);
    static CMyEngine* NewLC(TInt aSize);
    ~CMyEngine();

    void DoSomethingL();

private:
    CMyEngine(TInt aSize);        // phase 1
    void ConstructL();            // phase 2

private:
    TInt  iSize;
    TAny* iMemory;                // owned
    RFs   iFs;                    // owned handle
    };

// MyEngine.cpp
CMyEngine* CMyEngine::NewLC(TInt aSize)
    {
    CMyEngine* self = new (ELeave) CMyEngine(aSize);
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
    }

CMyEngine* CMyEngine::NewL(TInt aSize)
    {
    CMyEngine* self = CMyEngine::NewLC(aSize);
    CleanupStack::Pop(self);
    return self;
    }

CMyEngine::CMyEngine(TInt aSize) : iSize(aSize)
    {
    }

void CMyEngine::ConstructL()
    {
    iMemory = User::AllocL(iSize);
    User::LeaveIfError(iFs.Connect());
    }

CMyEngine::~CMyEngine()
    {
    iFs.Close();                  // safe even if Connect() never happened
    User::Free(iMemory);          // safe with NULL
    }
```

Usage:

```cpp
CMyEngine* engine = CMyEngine::NewLC(1024);   // on cleanup stack
engine->DoSomethingL();
CleanupStack::PopAndDestroy(engine);

iEngine = CMyEngine::NewL(1024);              // stored in a member: use NewL
```

* Use `NewLC` when the caller keeps the object on the stack (local use); `NewL` when storing it in a member.
* The destructor must cope with a **partially constructed** object (if `ConstructL()` left halfway) — thanks to CBase zero-fill, unset pointers are NULL and unopened handles are 0, so `delete`/`Close()` are safe.
* **Deriving from a two-phase class:** the derived `ConstructL()` should call `BaseConstructL()` (a protected phase-2 function of the base), rather than a private `ConstructL()`.
* Carbide.c++: *New → Symbian OS C++ Class → "Simple Symbian Style C++ Class"* generates this skeleton.

## 7.5 Cheat sheet

| I need… | Category | Create | Destroy |
|---------|----------|--------|---------|
| A small value | T | Stack: `TPoint p(1,2);` | Automatic |
| A heap object with logic/resources | C | `CFoo::NewL()` / `NewLC()` | `delete` / `PopAndDestroy` |
| A handle to a system resource | R | Stack/member, then `Open()`/`Connect()` | `Close()`; `CleanupClosePushL` |
| An interface/callback | M | Inherit after CBase | n/a |
| Utility functions | static | — | — |

## See also
* [Errors, leaves, panics and the cleanup stack](08-errors-leaves-cleanup.md)
* [Descriptors](09-descriptors.md)

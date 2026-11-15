# 9. Descriptors (Strings and Binary Buffers)

> **Scope:** all descriptor classes, literals, editing/searching/formatting, 8- vs 16-bit and character conversion, choosing a descriptor, descriptors as parameters/return values, packages (`TPckg`), and `TLex`.
> **Key facts:** descriptors know their length, are **not** zero-terminated, can hold binary data, **never grow automatically**, and **panic (`USER 11`) on overflow**.

## 9.1 Why descriptors

C strings were judged too dangerous (buffer overflows, leaks, unknown length). A descriptor stores its **length** (and, if modifiable, its **maximum length**) together with or alongside the data. For performance, overflow is not reported as an error but as a **panic** — you must size buffers correctly.

## 9.2 Class hierarchy

```
TDesC                         (abstract, read-only interface: Length, Ptr, Compare, Find, Left, Mid…)
├── TBufCBase
│   ├── TBufC<n>              constant buffer, data inline
│   └── HBufC                 constant buffer on the heap
├── TPtrC                     constant pointer to data elsewhere
└── TDes                      (abstract, modifiable interface: MaxLength, Append, Format, Copy…)
    ├── TBufBase
    │   └── TBuf<n>           modifiable buffer, data inline
    ├── TPtr                  modifiable pointer to data elsewhere
    └── RBuf                  modifiable, owns a heap buffer (R class)
TLitC<n>                      literal (from _LIT) — not derived from TDesC, but layout-compatible
```

* Classes ending in **`C`** are **non-modifiable** ("constant"): the data isn't edited in place, though the whole content may be replaced. They save 4 bytes (no max-length field).
* You **cannot instantiate `TDesC`/`TDes`**, and **you cannot create new descriptor types**: the type is encoded in 4 bits of the first word (the other 28 bits hold the length → max 256 MB) and `TDesC::Ptr()` switches on it. Methods are non-virtual (no vtable — another 4 bytes saved).
* Every class exists as **8-bit** (`TDesC8`, `TBuf8`, `TPtrC8`, `HBufC8`, `RBuf8`…) and **16-bit** (`TDesC16`, `TBuf16`…). The **neutral** names (`TDesC`, `TBuf`, …) are typedefs for the **16-bit** versions on all Symbian OS releases (Unicode/UCS-2). Use neutral names for text, explicit `8` classes for binary data and narrow text.

Memory layouts:

| Class | Layout |
|-------|--------|
| `TBufC<n>` / `HBufC` | `[type+length][data…]` |
| `TBuf<n>` | `[type+length][maxlength][data…]` |
| `TPtrC` | `[type+length][pointer]` |
| `TPtr` / `RBuf` | `[type+length][maxlength][pointer]` |

## 9.3 Literals: `_LIT` and `_L`

```cpp
_LIT(KMyName, "Aleksandr");        // TLitC<10> constant, can live in ROM; name with K prefix
_LIT8(KHeader, "GET / HTTP/1.0");  // 8-bit literal
const TDesC& d = KMyName;          // implicit conversion
TInt len = KMyName().Length();     // operator() gives a const TDesC&

User::InfoPrint(_L("Hi"));         // _L: unnamed, builds a TPtrC at run time
```

* Prefer **`_LIT`**. `_L` stores a zero-terminated string and constructs a `TPtrC` each time — 4 bytes more and slower. `_L` is **deprecated** outside test code (many book/SDK examples use it only for brevity).
* Empty descriptor: use **`KNullDesC`** / `KNullDesC8` (don't define your own empty literal).
* Escape sequences: `\\`, `\"`, `\0`, `\a`, `\b`, `\f`, `\n`, `\r`, `\t`, `\v`, `\xHH`, `\uXXXX`.

```cpp
_LIT(KPath, "c:\\data\\data.txt");     // backslashes MUST be doubled
_LIT(KApp,  "My\u00AE Application");    // "My® Application"
_LIT(KRu,   "\u0420\u0443\u0441\u0441\u043A\u0438\u0439");   // "Русский"
```

  A single `\` may still compile (e.g. `"c:\data"` → `\d` is unknown) and fail only at run time.
* Non-ASCII literals in source: either `\uXXXX` escapes (portable) or tell the compilers the source charset in the MMP, e.g. `OPTION GCCE -finput-charset=CP1251` and `OPTION CW -enc system` (or use UTF-8 source with matching options). Escapes are the safest.

## 9.4 Buffer descriptors: `TBuf<n>` and `TBufC<n>`

```cpp
const TInt KMaxNameLength = 20;
TBufC<KMaxNameLength> name(KMyName);   // initialised; content replaceable via =
TBuf<100> buf(_L("My name is: "));     // modifiable
buf.Append(name);                      // "My name is: Aleksandr"
TInt len = buf.Length();               // 21 characters
TInt max = buf.MaxLength();            // 100
TInt bytes = buf.Size();               // 42 bytes (16-bit!) — don't confuse Size() with Length()
```

* **Always check capacity before copying/appending** into a fixed buffer:

```cpp
if (src.Length() <= dst.MaxLength()) dst = src; else /* truncate or leave */;
dst.Copy(src.Left(dst.MaxLength()));       // explicit truncation
```

* Keep stack buffers small (≤ ~128 characters). Big system types: `TFileName` = `TBuf<256>` (**~520 bytes**), `TFullName`, `TName` (`TBuf<128>`). Sixteen `TFileName`s overflow an 8 KB stack. Put large buffers in **C-class members** or on the heap (`HBufC`/`RBuf`).

### Modifying methods (`TDes`)

| Method | Effect |
|--------|--------|
| `=`, `Copy(des)` | Replace content |
| `Append(des / TChar / ptr,len)`, `+=` | Append |
| `AppendNum(TInt [, TRadix])`, `AppendNum(TReal, TRealFormat)`, `AppendNumFixedWidth(...)` | Append a number (`EDecimal`, `EHex`, `EBinary`, `EOctal`) |
| `Num(...)` | `Zero()` + `AppendNum()` |
| `Format(fmt, ...)`, `AppendFormat(...)` | printf-like formatting |
| `Insert(pos, des)`, `Delete(pos, len)`, `Replace(pos, len, des)` | Edit |
| `Zero()` | Length = 0 |
| `SetLength(n)`, `SetMax()` | Change current length |
| `Fill(TChar [, len])`, `FillZ([len])`, `AppendFill(TChar, len)` | Fill (`FillZ` with zero bytes — length becomes max) |
| `Repeat(des)` | Fill whole buffer with repetitions |
| `Justify(des, width, ELeft/ECenter/ERight, fillChar)`, `AppendJustify(...)` | Pad and align |
| `Trim()`, `TrimLeft()`, `TrimRight()`, `TrimAll()` | Remove whitespace (`TrimAll` also collapses internal runs) |
| `LowerCase()`, `UpperCase()`, `Capitalize()`, `Fold()`, `Collate()` | Case/locale transforms |
| `Swap(TDes&)` | Exchange contents |
| `PtrZ()` | Append a `\0` (if room) and return a C-string pointer |
| `LeftTPtr(n)`, `RightTPtr(n)`, `MidTPtr(pos, n)` | Modifiable sub-pointers |

### `Format` directives (subset)

| Directive | Argument |
|-----------|----------|
| `%d`, `%i` | `TInt` |
| `%u` | `TUint` |
| `%x`, `%X` | hex |
| `%b`, `%o` | binary, octal |
| `%c` | character (`TUint`) |
| `%f`, `%e`, `%g` | `TReal` |
| `%Ld`, `%Lx` | `TInt64` |
| `%S` | **pointer to a descriptor** (`&des` or `&KLit`) — note capital S |
| `%s` | zero-terminated C string (`TText*`) |
| `%05d`, `%-8S`, `%+d`, `%*d` | width, padding, alignment |
| `%%` | literal % |

```cpp
_LIT(KFmt, "%S: %05d %S");
_LIT(KDist, "Distance");
_LIT(KKm, "km");
buf.Format(KFmt, &KDist, 170, &KKm);     // "Distance: 00170 km"
```

Passing a descriptor by value instead of by pointer to `%S` is a classic crash.

### Reading/searching methods (`TDesC`)

| Method | Returns |
|--------|---------|
| `Length()`, `Size()`, `Ptr()` | Characters, bytes, data pointer |
| `operator[](i)` | Character |
| `Left(n)`, `Right(n)`, `Mid(pos[, n])` | `TPtrC` to a sub-range (panics if out of range) |
| `Find(des)`, `FindF(des)` (folded), `FindC(des)` (collated) | Position or `KErrNotFound` |
| `Locate(TChar)`, `LocateReverse(TChar)`, `LocateF` | Position or `KErrNotFound` |
| `Match(pattern)`, `MatchF`, `MatchC` | Position or `KErrNotFound`; pattern with `*` (any run) and `?` (one char). There is no escape for literal `*`/`?`. |
| `Compare(des)`, `CompareF`, `CompareC`, `==`, `!=`, `<`, `>`, `<=`, `>=` | Lexicographic by character codes; a shorter prefix is "less" |
| `Alloc()`, `AllocL()`, `AllocLC()` | New `HBufC` copy |
| `HasPrefixC(...)` (newer) | — |

```cpp
_LIT(KNums, "1234567890");
TBuf<20> b(KNums);
TInt p1 = b.Find(_L("456"));     // 3
TInt p2 = b.Locate('2');         // 1
TInt p3 = b.Match(_L("*0"));     // 0 — matches; "*0?" -> KErrNotFound
```

## 9.5 Pointer descriptors: `TPtrC` and `TPtr`

Point at data owned by something else (another descriptor, a C array, ROM). They **never own memory** — the pointee must outlive them.

```cpp
TBuf<100> b(_L("0123456789"));
TPtrC p1;               // empty
p1.Set(b);              // p1 -> b's data
TPtrC p2(b.Mid(2, 3));  // "234"
TPtrC tail = b.Right(5);   // "56789"

TPtr w(const_cast<TUint16*>(b.Ptr()), b.Length(), b.MaxLength());   // modifiable view
TPtr sub = b.MidTPtr(5);  // modifiable view of "56789", max length 5
sub = _L("abc");          // b becomes "01234abc" — changes go to the original data
```

* **`Set()` re-points; `=` / `Copy()` copies data into the target.** Confusing them is a common bug: `ptr = other` on a `TPtr` copies into the memory `ptr` already points at (and panics if it doesn't fit).
* Typical use: splitting text into words without copying; returning a sub-range of a member or argument.
* Never return a `TPtr(C)` to a local (stack) buffer.

## 9.6 Heap descriptors: `HBufC` and `RBuf`

Use when the size is large or unknown at compile time.

### `HBufC`
* Non-modifiable (`TDesC` derived), allocated only on the heap (private constructor; the `H` stands for heap). Accessed through `HBufC*`.

```cpp
HBufC* h1 = HBufC::NewL(255);         // length 0, max 255 (may be rounded up)
HBufC* h2 = HBufC::NewMaxLC(512);     // length = max, contents uninitialised; on cleanup stack
HBufC* h3 = HBufC::New(100);          // returns NULL on failure (for non-leaving functions)
HBufC* copy = KMyName().AllocL();     // copy of any descriptor

*h1 = KMyName;                        // replace content (must fit)
TPtr p(h1->Des());                    // modifiable view for in-place editing
p.Append(_L(" Ivanov"));

const TDesC& ro = *h1;                // read access: just dereference — cheaper than Des()

h1 = h1->ReAllocL(1000);              // grow: may MOVE the buffer
p.Set(h1->Des());                     // so re-acquire any TPtr after ReAlloc
// NB: if h1 is on the cleanup stack, ReAllocL invalidates that entry — pop before, push after:
//     CleanupStack::Pop(h1); h1 = h1->ReAllocL(n); CleanupStack::PushL(h1);

delete h1;
CleanupStack::PopAndDestroy(h2);
```

* `HBufC` **does not grow automatically** — appending beyond max via `Des()` still panics `USER 11`.
* Using `Des()` where a `const TDesC&` would do is a common inefficiency.
* There is no `HBuf` class — use `RBuf` for a modifiable heap descriptor.

### `RBuf` (Symbian 8.x+)
* Modifiable (`TDes` derived) **and** an R class: it **owns** a heap buffer and must be `Close()`d.

```cpp
RBuf r;
r.CreateL(1024);                 // own empty buffer (also CreateMaxL, CreateL(des), CreateL(des, max))
r.CleanupClosePushL();           // or CleanupClosePushL(r)
r.Append(KMyName);
r.ReAllocL(2048);                // grow in place — RBuf updates itself
CleanupStack::PopAndDestroy(&r); // r.Close() frees the buffer

HBufC* h = HBufC::NewL(100);
RBuf r2;
r2.Assign(h);                    // take ownership of an HBufC (or RBuf r2(h);)
r2.Close();                      // frees h
```

`RBuf` is the recommended class for modifiable heap strings in new code.

## 9.7 8-bit vs 16-bit, encodings and conversion

* 8-bit: binary data, ASCII/Latin text, network protocols, files. 16-bit: Unicode (UCS-2) text — what the UI uses.
* `Length()` counts **characters**, `Size()` counts **bytes** (twice `Length()` for 16-bit). Mixing them up is a frequent bug (e.g. when writing to files or sockets).
* `Copy()` between widths does naïve conversion: 8→16 zero-extends each byte, 16→8 **drops the high byte**. Correct only for 7-bit ASCII / Latin-1:

```cpp
TBuf8<10> b8(_L8("ASCII"));
TBuf<10>  b16;
b16.Copy(b8);              // "ASCII" — fine
b8.Copy(_L("Unicode"));    // fine; Cyrillic/CJK would be destroyed
```

* **UTF-8/UTF-7 ↔ UCS-2:** `CnvUtfConverter` (`utf.h`, `charconv.lib`):

```cpp
TBuf8<100> utf8;
CnvUtfConverter::ConvertFromUnicodeToUtf8(utf8, ucs2);    // returns # unconverted chars or error
HBufC8* u8 = CnvUtfConverter::ConvertFromUnicodeToUtf8L(text);
HBufC*  u16 = CnvUtfConverter::ConvertToUnicodeFromUtf8L(*u8);
```

* **Other code pages:** `CCnvCharacterSetConverter` (`charconv.h`, `charconv.lib`, needs an `RFs`):

```cpp
RFs fs;
User::LeaveIfError(fs.Connect());
CleanupClosePushL(fs);
CCnvCharacterSetConverter* cnv = CCnvCharacterSetConverter::NewLC();
if (cnv->PrepareToConvertToOrFromL(KCharacterSetIdentifierCodePage1251, fs)
        != CCnvCharacterSetConverter::EAvailable)
    User::Leave(KErrNotSupported);
TBuf8<100> ansi(_L8("\xD0\xF3\xF1\xF1\xEA\xE8\xE9"));     // "Русский" in CP1251
TBuf<100> ucs2;
TInt state = CCnvCharacterSetConverter::KStateDefault;
User::LeaveIfError(cnv->ConvertToUnicode(ucs2, ansi, state));
CleanupStack::PopAndDestroy(2, &fs);   // cnv, fs
```

  Identifiers such as `KCharacterSetIdentifierUtf8`, `KCharacterSetIdentifierIso88591`, `KCharacterSetIdentifierCodePage1251` (`0x100059D7`), `KCharacterSetIdentifierShiftJis` are declared in `charconv.h`. `AutoDetectCharSetL()` can guess an encoding.

## 9.8 Choosing a descriptor

| C/C++ analogue | Symbian |
|---------------|---------|
| `static const char[]` | `_LIT` (`TLitC`) |
| `char[]` | `TBuf<n>` |
| `const char[]` | `TBufC<n>` |
| `char*` (to existing buffer) | `TPtr` |
| `const char*` | `TPtrC` |
| `char*` + `malloc` | `RBuf` (or `HBufC` + `Des()`) |
| `const char*` + `malloc` | `HBufC` |

Decision tree:

```
Need to modify it?
├─ Yes ─ Memory already exists elsewhere? ── Yes → TPtr
│                                        └── No → Size known and small (< ~256 bytes)? ── Yes → TBuf<n>
│                                                                                  └── No → RBuf (or HBufC + Des())
└─ No ── Memory already exists elsewhere (stack/heap/ROM)? ── Yes → TPtrC
                                                          └── No → Constant known at compile time? → _LIT
                                                                   Size known and small? ── Yes → TBufC<n>
                                                                                       └── No → HBufC
```

Binary data → the `8` variants (`TBuf8`, `HBufC8`, `RBuf8`, `TPtrC8`).

## 9.9 Descriptors as parameters and return values

* **Parameters:** use the abstract base classes **by reference** — `const TDesC&` for input, `TDes&` for output/in-out. They accept any descriptor type.

```cpp
void ConcatL(const TDesC& aA, const TDesC& aB, TDes& aResult)
    {
    if (aResult.MaxLength() < aA.Length() + aB.Length())
        User::Leave(KErrOverflow);
    aResult = aA;
    aResult.Append(aB);
    }
```

  Never pass `TDesC`/`TDes` by value — it compiles but slices away the data.
* **Returning a reference** is fine only for data that outlives the call (a member): `const TDesC& Name() const { return *iName; }`.
* **Returning a new heap descriptor:** return `HBufC*` and document that the caller takes ownership; for leaving functions consider an `LC` variant that leaves it on the cleanup stack:

```cpp
HBufC* ConcatLC(const TDesC& aA, const TDesC& aB)
    {
    HBufC* res = HBufC::NewLC(aA.Length() + aB.Length());
    TPtr p(res->Des());
    p = aA;
    p.Append(aB);
    return res;                       // caller must PopAndDestroy
    }
```

* Returning `TPtrC` to part of an argument or member is fine: `TPtrC FirstHalf(const TDesC& aDes) { return aDes.Left(aDes.Length() / 2); }`
* Returning `TBuf<n>` by value works but costs stack and caps the size — avoid.
* Output parameter convention: `void GetName(TDes& aName) const;`

## 9.10 Package descriptors: `TPckg`, `TPckgC`, `TPckgBuf`

Wrap any (T-class) object as an **8-bit descriptor** so it can be written to a file/stream or passed across threads/processes (client-server IPC).

```cpp
class TConfig
    {
public:
    TInt     iVolume;
    TBuf<16> iName;
    };

TConfig cfg;
TPckg<TConfig>    pkg(cfg);        // TPtr8 over cfg — modifies cfg directly (no copy)
TPckgC<TConfig>   cpkg(cfg);       // TPtrC8 over cfg — read-only
TPckgBuf<TConfig> buf(cfg);        // TBuf8 holding a COPY of cfg

TConfig& inside = buf();           // operator() gives access to the packaged object
TConfig copy = cpkg();
```

Only package flat T-classes (no pointers, no heap ownership) — the bytes are copied verbatim.

## 9.11 Parsing with `TLex`

`TLex` (`TLex16`; also `TLex8`) is a lexical analyser over a descriptor: converts text to numbers and tokenises.

```cpp
_LIT(KInt, "215");
_LIT(KHex, "0F00");
_LIT(KReal, "3.14");

TLex lex(KInt);
TInt32 i = 0;
User::LeaveIfError(lex.Val(i));              // 215

lex.Assign(KHex);
TUint u = 0;
User::LeaveIfError(lex.Val(u, EHex));        // 3840

lex.Assign(KReal);
TReal r = 0;
User::LeaveIfError(lex.Val(r));              // 3.14 (uses locale decimal separator; Val(r, '.') to force)

// Tokenising
TLex words(_L("alpha beta  gamma"));
while (!words.Eos())
    {
    TPtrC token = words.NextToken();         // skips whitespace
    if (token.Length() == 0) break;
    }
// Lower level: Peek(), Get(), Inc(), Mark(), MarkedToken(), SkipSpace(), SkipCharacters(), Offset()
```

* `Val()` returns `KErrNone`, `KErrGeneral` (no number) or `KErrOverflow`. Overloads exist for `TInt8/16/32/64`, unsigned with `TRadix`, with limits, and `TReal32/64`.
* Number → text: `TDes::Num()` / `AppendNum()`.
* Example code: `\Examples\Base\BufsAndStrings\Lexer\` in the SDK.

## 9.12 `LString` (EUserHL)

Symbian^3's EUserHL library adds `LString` (derived from `RBuf`), the only descriptor that **grows automatically**. It requires `euserhl.dll` on the device (a separate SIS for 9.1–9.4). Convenient, but learn the classic descriptors first. See [Errors and cleanup](08-errors-leaves-cleanup.md#88-l-classes-euserhl).

## 9.13 Common descriptor bugs

| Bug | Fix |
|-----|-----|
| `USER 11` panic | Check `MaxLength()`; use `RBuf`/`HBufC` with the right size; `ReAllocL` |
| `%s` used for a descriptor in `Format` | Use `%S` and pass `&des` |
| Passing `TDesC` by value | `const TDesC&` |
| Returning `TPtrC`/reference to a local buffer | Return `HBufC*` or use a member |
| Using `TPtr` from `Des()` after `ReAllocL` | Re-acquire with `Set(h->Des())` |
| `Size()` vs `Length()` confusion in I/O | `Size()` = bytes, `Length()` = characters |
| `Copy()` 16→8 of non-Latin text | `CnvUtfConverter` / `CCnvCharacterSetConverter` |
| Many `TFileName` on the stack | Members, heap, or `RBuf` |
| Single backslash in paths | `"c:\\data\\file.txt"` |
| `_L` in production code | `_LIT` |

## See also
* [Arrays](10-arrays.md) — `CDesCArray` descriptor arrays
* [Streams](19-streams-stores.md) — externalising descriptors

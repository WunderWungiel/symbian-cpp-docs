# 10. Arrays and Collections

> **Scope:** dynamic arrays (`RArray`, `RPointerArray`, the `CArrayX` family, descriptor arrays), fixed arrays (`TFixedArray`), sorting and searching, plus a note on other containers.
> **Default choice:** `RArray<T>` for small flat T-types, `RPointerArray<T>` for owned C-objects, `CDesCArrayFlat` for lists of strings.

## 10.1 Concepts

* **Flat buffer** — all elements in one contiguous heap cell. Fast indexed access; insert/delete/grow may move memory.
* **Segmented buffer** — a linked list of heap segments. Cheap inserts/appends; slower random access. Prefer when elements are frequently inserted/removed.
* **Granularity** — how many element slots are added each time the buffer grows. A new array allocates nothing; the first append reserves `granularity` slots. Too small → many reallocations; too large → wasted memory. If you know the final size, use it (or a divisor of it). `Compress()` frees unused slots.

## 10.2 `RArray<T>` and `RPointerArray<T>` (recommended)

Efficient R-class arrays with flat buffers. `RArray` stores elements **by value**; `RPointerArray` stores **pointers**.

`RArray` element restrictions: size **≥ 4 and ≤ 640 bytes, a multiple of 4** (`RArray<TInt>` / `RArray<TUint>` are specialised and have simpler sort/find).

```cpp
RArray<TInt> arr(16);                  // granularity 16 (default 8)
CleanupClosePushL(arr);
for (TInt i = 0; i < 30; ++i)
    arr.AppendL(Math::Random());       // leaving variant…
TInt err = arr.Append(42);             // …or error-returning variant (for non-leaving code)
arr.Sort();                            // ascending
TInt pos = arr.Find(255);              // index or KErrNotFound
TInt idx = arr.SpecificFindInOrder(42, EArrayFindMode_First);   // binary search in sorted array
TInt sum = 0;
for (TInt i = 0; i < arr.Count(); ++i) sum += arr[i];           // [] panics USER 130 if out of range
arr.Remove(0);
arr.Compress();
CleanupStack::PopAndDestroy(&arr);     // arr.Close()
```

Key members: `Count()`, `operator[]`, `Append[L]()`, `Insert[L](obj, pos)`, `InsertInOrder[L]()`, `InsertInOrderAllowRepeats[L]()`, `Remove(pos)`, `Find()`, `FindInOrder()`, `FindReverse()`, `Sort()`, `Reset()` (empty, free memory), `Close()` (free memory; must be called), `Reserve(n)` (newer), `Array()` (→ `TArray<T>` generic view).

### `RPointerArray<T>`

```cpp
RPointerArray<CItem> items;
CleanupResetAndDestroyPushL(items);    // on leave: delete every element + free array
CItem* item = CItem::NewLC();
items.AppendL(item);                   // may leave — item still on cleanup stack
CleanupStack::Pop(item);               // now owned by the array
...
items.ResetAndDestroy();               // delete all elements, free buffer (owned elements!)
CleanupStack::Pop(&items);             // or PopAndDestroy(&items)
// items.Reset()/Close() would free only the array, not the objects
```

`CleanupResetAndDestroyPushL()` is declared in `mmf/common/mmfcontrollerpluginresolver.h` (S60 3rd) — or write your own `TCleanupItem` that calls `ResetAndDestroy()`. As a class member, call `iItems.ResetAndDestroy()` in the destructor.

### Sorting and searching custom types

Provide comparison functions and wrap them in `TLinearOrder<T>` (ordering: return <0, 0, >0) or `TIdentityRelation<T>` (equality: return `TBool`):

```cpp
struct TMyPosition
    {
    TInt iX;
    TInt iY;
    TMyPosition(TInt aX, TInt aY) : iX(aX), iY(aY) {}
    };

static TInt CompareByX(const TMyPosition& aA, const TMyPosition& aB)
    {
    return aA.iX - aB.iX;
    }

static TBool IsNear(const TMyPosition& aA, const TMyPosition& aB)
    {
    return Abs(aA.iX - aB.iX) < 10 && Abs(aA.iY - aB.iY) < 10;
    }

RArray<TMyPosition> arr;               // 8-byte element: OK
CleanupClosePushL(arr);
arr.AppendL(TMyPosition(100, 100));
arr.AppendL(TMyPosition(6, 7));
arr.Sort(TLinearOrder<TMyPosition>(CompareByX));
TInt pos = arr.Find(TMyPosition(101, 101), TIdentityRelation<TMyPosition>(IsNear));   // finds (100,100)
CleanupStack::PopAndDestroy(&arr);
```

For key-based lookup in structs there are also `Find(const T&, TInt aKeyOffset)` overloads and `_FOFF(TMyPosition, iX)` to compute a member offset.

### Pitfall: copying R arrays
`=` on `RArray`/`RPointerArray` copies the **handle**, not the elements: both objects then refer to the same buffer, and when one reallocates or closes, the other dangles. Pass arrays by reference; never copy them.

## 10.3 The `CArrayX` family

Older C-class arrays, named `CArray<Elem><Buf>`:

| `<Elem>` | Elements | | `<Buf>` | Buffer |
|----------|----------|-|---------|--------|
| `Fix` | Fixed-size, stored in the buffer | | `Flat` | Contiguous |
| `Var` | Variable-size, each in its own heap cell (buffer holds pointers) | | `Seg` | Segmented |
| `Pak` | Variable-size, packed in the buffer with a length prefix | | | |
| `Ptr` | Pointers to (C-class) objects | | | |

→ `CArrayFixFlat<T>`, `CArrayFixSeg<T>`, `CArrayVarFlat<T>`, `CArrayVarSeg<T>`, `CArrayPakFlat<T>`, `CArrayPtrFlat<T>`, `CArrayPtrSeg<T>` (and `CArrayFix<T>` etc. as bases).

```cpp
CArrayFixFlat<TUint>* arr = new (ELeave) CArrayFixFlat<TUint>(10);   // granularity
CleanupStack::PushL(arr);
arr->AppendL(Math::Random());
TUint first = (*arr)[0];             // or arr->At(0)
TKeyArrayFix key(0, ECmpTUint32);    // key: offset 0 in element, compare as TUint32
arr->Sort(key);
TInt pos;
if (arr->Find(100, key, pos) == 0) { /* found at pos */ }            // Find returns 0 when found
CleanupStack::PopAndDestroy(arr);
```

Common members: `Count()`, `AppendL()`, `InsertL(pos, ...)`, `ExtendL()` / `ExpandL(pos)` (append/insert default-constructed element and return a reference), `At()`/`[]`, `Delete(pos[, count])`, `Compress()`, `ResizeL(n)`, `Reset()`, `Length(i)` (element size; `Var`/`Pak`), `Sort(key)`, `Find(item, key, pos)`, `FindIsq()` (binary search), `InsertIsqL()` (sorted insert).

* Keys: `TKeyArrayFix(offset, ECmpTInt / ECmpTUint32 / ECmpNormal / ECmpFolded / ECmpCollated …[, length])`, `TKeyArrayVar`, `TKeyArrayPak`.
* `CArrayPtr*` arrays don't own their objects by default; call **`ResetAndDestroy()`** to delete them.
* References returned by `At()`/`[]` become invalid after the array changes.

**When to still use CArrayX:** segmented buffers (frequent insertion), variable-length or packed elements, elements > 640 bytes, and APIs that require them (many UI APIs take `CArrayFix<TInt>` / `CDesCArray`).

## 10.4 Descriptor arrays (BAFL)

Header `badesca.h`, library **`bafl.lib`**. Not templates.

| Class | Stores |
|-------|--------|
| `CDesCArrayFlat`, `CDesCArraySeg` (= `CDesC16Array*`) | **Copies** of 16-bit strings (each in its own `HBufC`) |
| `CDesC8ArrayFlat`, `CDesC8ArraySeg` | Copies of 8-bit strings |
| `CPtrCArray` (`CPtrC16Array`), `CPtrC8Array` | `TPtrC`s pointing at strings owned elsewhere |
| `CDesCArray` / `MDesCArray` | Base class / interface (list boxes take `MDesCArray*`) |

```cpp
CDesCArrayFlat* names = new (ELeave) CDesCArrayFlat(5);
CleanupStack::PushL(names);
names->AppendL(_L("Test string"));
names->AppendL(_L("Another string"));
names->InsertIsqL(_L("Middle"));               // sorted insert
TPtrC third = (*names)[2];                     // or names->MdcaPoint(2)
names->Sort();                                 // or Sort(ECmpFolded), Sort(ECmpCollated)
TInt pos;
if (names->Find(_L("Middle"), pos) == 0) { /* found */ }
names->Delete(0);
CleanupStack::PopAndDestroy(names);
```

S60 list boxes typically use a `CDesCArray` as their item array, with tab-separated fields: `"\tTitle\tSubtitle"`, `"0\tTitle"` (icon index first).

## 10.5 Fixed-size arrays: `TFixedArray<T, N>`

A bounds-checked replacement for C arrays (prefer it):

```cpp
TFixedArray<TInt, 5> arr;
for (TInt i = 0; i < arr.Count(); ++i) arr[i] = i;     // [] / At(): USER 133 in UDEB if out of range

TInt data[] = {0, 1, 2, 3, 4};
TFixedArray<TInt, 5> arr2(&data[0], 5);                 // copy from a C array
```

Members: `At()`/`[]`, `Count()`, `Length()` (element size), `Copy(ptr, n)`, `Begin()`, `End()`, `Reset()` (zero-fill), `DeleteAll()` (delete every pointer element; then `Reset()`), `Array()`.

## 10.6 Other containers (brief)

| Class | Purpose |
|-------|---------|
| `TSglQue<T>` / `TDblQue<T>` + `TSglQueIter` / `TDblQueIter` | Intrusive singly/doubly-linked lists (elements embed a `TSglQueLink`/`TDblQueLink`; set the offset with `_FOFF`) |
| `TDeltaQue<T>` | Delta-ordered queue (timers) |
| `CCirBuf<T>`, `CCirBuffer` | Circular buffers |
| `CBufFlat`, `CBufSeg` (`CBufBase`) | Dynamic **byte** buffers (`InsertL`, `Read`, `Write`, `Ptr`) — useful with `RBufWriteStream` |
| `RHashMap<K,V>`, `RHashSet<T>`, `RPtrHashMap<K,V>` (9.x, `e32hashtab.h`) | Hash tables |
| `TBtreeFix<Entry,Key>` | B-trees (store-backed) |

## See also
* [Descriptors](09-descriptors.md)
* [Errors and cleanup](08-errors-leaves-cleanup.md) — `CleanupClosePushL`, `CleanupResetAndDestroyPushL`

# 19. Streams and Stores

> **Scope:** the stream framework (`RReadStream`/`RWriteStream`, file/memory/descriptor streams, `<<`/`>>`, `ExternalizeL`/`InternalizeL`, `TCardinality`) and stores (`CDirectFileStore`, `CPermanentFileStore`, `CDictionaryFileStore`, root streams, `CStreamDictionary`, compaction, `TSwizzle`).
> **Headers / libs:** `s32strm.h`, `s32file.h`, `s32mem.h`, `s32std.h`; **`estor.lib`** (+ `efsrv.lib` for files).

## 19.1 Concepts

* A **stream** is the external (serialised) representation of one or more objects, plus the code to **externalise** (save) and **internalise** (restore) them.
* The representation must be complete and self-contained: store data, not pointers or open handles (store a file *name*, not an `RFile`).
* It must be storage-independent with fixed sizes and byte order — e.g. write `TInt32`, not `TInt`, conceptually; read back in exactly the order written.
* Streams are read/written through **read streams** and **write streams** wrapping a **stream buffer** (`MStreamBuf`; buffered ones derive from `TStreamBuf`). Concrete buffers: `RFileBuf` (files), `TMemBuf` (memory), `TDesBuf` (descriptors), `RShareBuf`… Stream buffers have **independent read and write positions**.
* Streams are generally preferable to raw `RFile`: they're **buffered** (far fewer IPC calls), typed and composable.

## 19.2 Base stream API

`RReadStream`:

| Method | Reads |
|--------|-------|
| `ReadInt8L()`, `ReadInt16L()`, `ReadInt32L()` | signed integers |
| `ReadUint8L()`, `ReadUint16L()`, `ReadUint32L()` | unsigned integers |
| `ReadReal32L()`, `ReadReal64L()` | floating point |
| `ReadL(TDes8&/TDes16& [, length])`, `ReadL(TDes&, TChar delim)` | raw descriptor data (fills it, a given length, or up to a delimiter) |
| `ReadL(TAny* ptr, length)`, `ReadL(RWriteStream&, length)` | raw bytes into memory / pipe into a write stream |
| `ReadL(TInt aLength)` | skip bytes |

`RWriteStream`: the matching `WriteInt8L()`…`WriteReal64L()`, `WriteL(const TDesC8&/TDesC16& [, length])`, `WriteL(ptr, length)`, `WriteL(RReadStream&, length)`, and **`CommitL()`** (flush the buffer to the medium; done automatically on close for file streams).

Both: `PushL()`/`Pop()` (put the stream on the cleanup stack), `Close()`/`Release()`, `Source()`/`Sink()` → `MStreamBuf*` with `SeekL()`, `TellL()`, `SizeL()`.

All reads **leave** with `KErrEof` at end of stream — every function using them must be leave-safe (`L` suffix).

## 19.3 File streams

```cpp
// Writing
RFileWriteStream out;
User::LeaveIfError(out.Replace(fs, KFileName, EFileWrite));   // also Open(), Create(), Temp(), or Attach(RFile&)
out.PushL();
out.WriteInt32L(items.Count());
for (TInt i = 0; i < items.Count(); ++i)
    {
    out.WriteInt32L(items[i].Length());
    out.WriteL(items[i]);
    }
out.CommitL();
CleanupStack::PopAndDestroy(&out);

// Reading
RFileReadStream in;
User::LeaveIfError(in.Open(fs, KFileName, EFileRead | EFileShareReadersOnly));
in.PushL();
TInt count = in.ReadInt32L();
HBufC8* buf = HBufC8::NewLC(64);
for (TInt i = 0; i < count; ++i)
    {
    TInt len = in.ReadInt32L();
    if (len > buf->Des().MaxLength())
        {
        CleanupStack::Pop(buf);
        buf = buf->ReAllocL(len);       // HBufC may move: re-push
        CleanupStack::PushL(buf);
        }
    TPtr8 p(buf->Des());
    in.ReadL(p, len);
    // use *buf
    }
CleanupStack::PopAndDestroy(2, &in);   // buf, in
```

`Attach(RFile& file [, pos])` (or the matching constructor) takes over an already open `RFile` — the `RFile` handle must not be used afterwards.

## 19.4 Memory and descriptor streams

```cpp
const TInt KSize = 0x1000;
TAny* mem = User::AllocLC(KSize);

RMemWriteStream w(mem, KSize);       // or Open(mem, size)
w.PushL();
w.WriteInt8L(0x10);
CleanupStack::PopAndDestroy(&w);

RMemReadStream r(mem, KSize);
r.PushL();
TInt v = r.ReadInt8L();
CleanupStack::PopAndDestroy(2, mem); // closing a mem stream does NOT free the memory
```

* `RDesWriteStream` / `RDesReadStream` (`s32mem.h`) serialise into/out of an **8-bit descriptor** (`TDes8`/`TDesC8`) — handy with `HBufC8` for IPC or databases.
* `RBufWriteStream` / `RBufReadStream` work over a dynamic `CBufBase` (`CBufFlat`, `CBufSeg`) that **grows** as you write — the easiest way to serialise data of unknown size:

```cpp
CBufFlat* buf = CBufFlat::NewL(256);
CleanupStack::PushL(buf);
RBufWriteStream ws(*buf);
ws.PushL();
ws << *iName << TInt32(iCount);
ws.CommitL();
CleanupStack::PopAndDestroy(&ws);
TPtr8 bytes = buf->Ptr(0);          // serialised data
CleanupStack::PopAndDestroy(buf);
```

## 19.5 Operators `<<` and `>>`, `ExternalizeL` / `InternalizeL`

`<<` and `>>` are defined for the basic types (`TInt8`…`TInt32`, `TUint8`…`TUint32`, `TReal32`, `TReal64`, `TInt64`, `TUid`, `TCheckedUid`, `TStreamId`…) **and descriptors**. They can leave.

```cpp
ws << TInt32(0x10);
ws << _L("Line");       // writes a compressed length header + data
TInt32 i;
rs >> i;
TBuf<10> buf;
rs >> buf;              // max length must be large enough (else leaves/panics)
HBufC* h = HBufC::NewL(rs, KMaxLen);   // read a descriptor of unknown length directly into an HBufC (also NewLC)
```

**Prefer `<<`/`>>` for descriptors** — the length prefix is compact (`TCardinality` encoding) and the read side knows how much to read.

Any class gets `<<`/`>>` support by implementing two members:

```cpp
class TMyStreamData
    {
public:
    void ExternalizeL(RWriteStream& aStream) const;
    void InternalizeL(RReadStream& aStream);
public:
    TBuf<0x100> iDes;
    TInt32      iInt32;
    TReal64     iReal64;
    };

void TMyStreamData::ExternalizeL(RWriteStream& aStream) const
    {
    aStream << iDes;
    aStream << iInt32;
    aStream << iReal64;
    }

void TMyStreamData::InternalizeL(RReadStream& aStream)
    {
    aStream >> iDes;
    aStream >> iInt32;
    aStream >> iReal64;
    }

// usage
out << data;          // calls data.ExternalizeL(out)
in  >> data2;         // calls data2.InternalizeL(in)
```

For C-classes, a common pattern is `static CFoo* NewL(RReadStream& aStream)` that constructs and internalises. Put a **version number** first in your stream format so future versions can read old files.

`TCardinality` writes non-negative integers compactly (1 byte for 0–127, 2 bytes up to 2^14−1, else 4 bytes):

```cpp
ws << TCardinality(count);
TCardinality c;
rs >> c;
TInt count2 = c;
```

## 19.6 Stores

A **store** is a collection of streams identified by **`TStreamId`**, usually in a file. Great for documents containing many independently-loadable parts; supports commit/revert (transactions).

```
CStreamStore (abstract)
├── CBufStore                     non-persistent, in memory
├── CEmbeddedStore                a store inside a stream of another store
├── CSecureStore                  encrypted streams
└── CPersistentStore (abstract)   adds the ROOT stream
    └── CFileStore (abstract)     file-based; UIDs
        ├── CDirectFileStore      write-once: streams can't be changed after commit
        └── CPermanentFileStore   streams can be replaced/deleted; needs compaction
CDictionaryFileStore              wraps a permanent store + automatic dictionary (separate hierarchy)
```

* **Direct file store**: write everything once, read many times; to update, rewrite the whole file. Good for read-only data (game levels) or apps that keep the full model in RAM and save it all (settings, simple documents).
* **Permanent file store**: streams can be modified in place (old versions are just marked free) — for databases and editors that load parts on demand. Gets fragmented: `ReclaimL()` (mark free space reusable) and `CompactL()` (shrink the file), both return free bytes; `RStoreReclaim` does it incrementally.

A store file starts with three UIDs: **UID1 = layout** (`KDirectFileStoreLayoutUid` `0x10000037`, `KPermanentFileStoreLayoutUid` `0x10000050`); UID2/UID3 are yours (e.g. format version and your app UID) — check them when opening.

### Creating a store

```cpp
_LIT(KStoreName, "c:\\private\\E1234567\\store.dat");
const TUid KMyIntStreamUid = { 0x101010FF };

CPermanentFileStore* store = CPermanentFileStore::ReplaceLC(fs, KStoreName, EFileWrite);
store->SetTypeL(TUidType(store->Layout(), KMyFormatUid, KMyAppUid));

// a data stream
RStoreWriteStream data;
TStreamId dataId = data.CreateLC(*store);
data << TInt32(10);
data.CommitL();                              // NOT automatic on close for store streams!
CleanupStack::PopAndDestroy(&data);

// the root stream holds a dictionary: UID -> stream id
CStreamDictionary* dict = CStreamDictionary::NewLC();
dict->AssignL(KMyIntStreamUid, dataId);

RStoreWriteStream root;
TStreamId rootId = root.CreateLC(*store);
root << *dict;
root.CommitL();
store->SetRootL(rootId);
CleanupStack::PopAndDestroy(2, dict);        // root, dict

store->CommitL();                            // commit the store transaction
CleanupStack::PopAndDestroy(store);
```

* `CreateL/LC`, `ReplaceL/LC`, `TempL/LC`, `OpenL/LC`, `FromL(RFile&)` mirror `RFile`.
* Each `RStoreWriteStream` must be `CommitL()`ed before closing; `store->CommitL()` makes all changes durable, `store->Revert[L]()` rolls back to the last commit.
* `CStreamDictionary` maps your own **UIDs** to stream IDs (which you can't know in advance); it's the recommended content of the root stream. `At(uid)`, `AssignL(uid, id)`, `Remove(uid)`, `IsNull()`.

### Opening and reading

```cpp
CPermanentFileStore* store = CPermanentFileStore::OpenLC(fs, KStoreName, EFileRead);
// optionally check store->Type()[1], [2]

RStoreReadStream root;
root.OpenLC(*store, store->Root());
CStreamDictionary* dict = CStreamDictionary::NewLC();
root >> *dict;
TStreamId dataId = dict->At(KMyIntStreamUid);
CleanupStack::PopAndDestroy(2, &root);       // dict, root

RStoreReadStream data;
data.OpenLC(*store, dataId);
TInt32 value;
data >> value;
CleanupStack::PopAndDestroy(2, store);       // data, store
```

Open a store with the **same class** that created it (or the generic `CFileStore::OpenLC()`, which picks the right one from UID1).

Modifying a permanent store: `RStoreWriteStream::ReplaceLC(*store, id)` to rewrite a stream, `OpenLC()` to modify, `store->DeleteL(id)` to delete; then `CommitL()`.

### `CDictionaryFileStore`

Hides root stream and dictionary management — streams are addressed directly by UID. Typical for INI-style settings files (the GUI framework uses one for application `.ini` files: `CEikApplication::OpenIniFileLC()`).

```cpp
CDictionaryFileStore* store = CDictionaryFileStore::OpenLC(fs, KIniName, KMyAppUid);  // creates if missing; checks UID3
RDictionaryWriteStream w;
w.AssignLC(*store, KMyIntStreamUid);       // creates or replaces the stream with that UID
w << TInt32(10);
w.CommitL();
CleanupStack::PopAndDestroy(&w);
store->CommitL();

if (store->IsPresentL(KMyIntStreamUid))
    {
    RDictionaryReadStream r;
    r.OpenLC(*store, KMyIntStreamUid);
    TInt32 v;
    r >> v;
    CleanupStack::PopAndDestroy(&r);
    }
CleanupStack::PopAndDestroy(store);
```

`IsNullL()` — dictionary empty; `IsPresentL(uid)`; `Remove(uid)`.

### `TSwizzle` — lazy loading
`TSwizzle<T>` / `TSwizzleC<T>` hold **either a `TStreamId`** (object not loaded) **or a pointer** (loaded), letting complex documents load embedded objects on demand: `IsId()`, `IsPtr()`, `AsId()`, `AsPtr()`. See the SDK help (*Store → Swizzles*).

## 19.7 When to use what

| Need | Use |
|------|-----|
| A few settings | Central Repository (system-managed), or `CDictionaryFileStore` / a simple file stream in your private dir |
| Serialise objects to a file once | `RFileWriteStream` + `ExternalizeL` |
| A document with independently loaded parts | `CPermanentFileStore` (+ `CStreamDictionary`, swizzles) |
| Pass a structure across IPC | `RDesWriteStream` into an `HBufC8`, or `TPckg` for flat T-classes |
| Large structured data with queries | DBMS / Symbian SQL ([Databases](20-databases.md)) |

## See also
* [File system](18-file-system.md)
* [Databases](20-databases.md)

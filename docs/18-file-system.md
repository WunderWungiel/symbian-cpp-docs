# 18. The File System: `RFs`, `TParse`, `CFileMan`, `RFile`

> **Scope:** file naming and drives, the file server session, private directories, drive/volume info, directories and attributes, listing, `CFileMan`, `RFile` (open modes, reading, writing, seeking, locking), `TFileText`.
> **Headers / libs:** `f32file.h`, `efsrv.lib`. Platform-security rules: see [data caging](02-platform-security.md#24-data-caging).

## 18.1 Names and drives

* Full name: `drive:\path\name.ext`, **max 256 characters** (`KMaxFileName` = 0x100), case-insensitive. Store in `TFileName` (= `TBuf<256>`, ~520 bytes — avoid many on the stack).
* Backslash separators — doubled in literals: `_LIT(KFile, "c:\\path1\\path2\\file.ext");`
* Forbidden: `< > " / |`, a double backslash, `:` except after the drive letter, wildcards `* ?` in directory names, names made only of spaces, a space between drive and path, `.` and `..` as names.
* Typical drives: `C:` phone memory, `D:` RAM disk (S60, volatile), `E:` memory card (UIQ: `D:`), `Z:` ROM. Don't hard-code — enumerate drives (§18.4) or use the drive your app was installed on:

```cpp
TParsePtrC exe(RProcess().FileName());
TPtrC myDrive = exe.Drive();                 // e.g. "e:"
```

  (S60 also offers `PathInfo::PhoneMemoryRootPath()`, `MemoryCardRootPath()`, `ImagesPath()` etc. in `pathinfo.h`, `platformenv.lib`.)
* **Drive search order** for lookups without a drive letter (e.g. `SetSessionPath()` without a drive, `!:` in startup items): before 9.x it was alphabetical, which let a file on `C:` shadow a ROM component on `Z:`. From 9.x the server searches **`Z:` first**, then the other drives alphabetically.

## 18.2 `TParse` — parsing names

```cpp
_LIT(KFile, "c:\\Path1\\Path2\\FileName.Ext");
TParse p;
p.Set(KFile, NULL, NULL);       // (name, related, default)
p.Drive();          // "c:"
p.Path();           // "\Path1\Path2\"
p.DriveAndPath();   // "c:\Path1\Path2\"
p.Name();           // "FileName"
p.Ext();            // ".Ext"
p.NameAndExt();     // "FileName.Ext"
p.FullName();       // the whole thing
p.AddDir(_L("Sub"));   // insert a directory before the name
p.PopDir();            // remove the last directory
```

* `Set(name, related, default)` fills missing components from the *related* name, then the *default* name: `p.Set(_L("a:file1"), NULL, &_L("c:\\path1\\*.*"))` → `a:\path1\file1.*`.
* `SetNoWild()` rejects wildcards. Presence checks: `DrivePresent()`, `PathPresent()`, `NamePresent()`, `ExtPresent()`, `NameOrExtPresent()`, `IsRoot()`, `IsWild()`, `IsNameWild()`, `IsExtWild()`, `IsKMatchAny()`, `IsKMatchOne()`.
* `TParsePtr` / `TParsePtrC` don't copy the name (they point at your descriptor) — cheaper, no 512-byte buffer.
* `RFs::Parse(name, TParse&)` / `Parse(name, related, TParse&)` use the session path as default.

## 18.3 The file server session `RFs`

```cpp
RFs fs;
User::LeaveIfError(fs.Connect());
CleanupClosePushL(fs);
// ...
CleanupStack::PopAndDestroy(&fs);
```

* **Use as few sessions as possible**, connected for the app's lifetime (each `Connect()` is IPC + kernel objects). GUI apps already have one: `CCoeEnv::Static()->FsSession()` / `iCoeEnv->FsSession()` in AppUi/controls (`RFs& iFs` member initialised from it). Don't `Close()` that one.
* **Session path:** one current directory per session (not per drive). Defaults to the process's **private path**. `SessionPath(TDes&)`, `SetSessionPath(path)` (must end with `\`; setting it to `\sys\` or another private dir needs `AllFiles`).
* `IsValidName(name [, TText& badChar])` validates syntax.

### Private directory

```cpp
TFileName priv;
fs.PrivatePath(priv);                       // "\private\E1234567\" (no drive)
fs.CreatePrivatePath(EDriveC);              // create it (needed in the emulator; the installer creates it on devices)
fs.SetSessionToPrivate(EDriveC);            // session path = c:\private\<SID>\
```

Emulator-safe pattern:

```cpp
TFileName path;
User::LeaveIfError(fs.PrivatePath(path));
TParsePtrC exe(RProcess().FileName());
path.Insert(0, exe.Drive());
TInt err = fs.MkDirAll(path);
if (err != KErrNone && err != KErrAlreadyExists) User::Leave(err);
```

## 18.4 Drives and volumes

```cpp
TDriveList drives;                          // TBuf8<KMaxDrives> (26)
User::LeaveIfError(fs.DriveList(drives));
for (TInt d = EDriveA; d <= EDriveZ; ++d)
    {
    if (!drives[d]) continue;
    TChar letter;
    fs.DriveToChar(d, letter);              // and RFs::CharToDrive(letter, d)
    TDriveInfo info;
    fs.Drive(info, d);                      // iType (EMediaRam/Flash/HardDisk/Rom/NotPresent…), iDriveAtt, iMediaAtt
    TVolumeInfo vol;
    if (fs.Volume(vol, d) == KErrNone)      // KErrNotReady if no media
        {
        TInt64 total = vol.iSize;
        TInt64 free  = vol.iFree;
        // vol.iName = label, vol.iUniqueID
        }
    }
```

* Drive attributes: `KDriveAttLocal`, `KDriveAttRom`, `KDriveAttRemovable`, `KDriveAttInternal`, `KDriveAttRemote`, `KDriveAttSubsted`, `KDriveAttRedirected`. Media attributes: `KMediaAttWriteProtected`, `KMediaAttLocked`, `KMediaAttFormattable`…
* `GetDriveName(drive, name)` — volume label (often empty, e.g. `"MultiMediaCard0"`).
* Check free space **before** writing large files (`SysUtil::DiskSpaceBelowCriticalLevelL()` on S60, `sysutil.lib`) — the phone misbehaves when `C:` is full.

## 18.5 Directories, renaming, deleting, attributes

All return `TInt` errors (no leaves) — **check them**.

| Call | Notes |
|------|-------|
| `MkDir(path)` | Parent must exist; path must end with `\`; `KErrAlreadyExists` if present |
| `MkDirAll(path)` | Creates intermediate dirs; `KErrAlreadyExists` if the final one exists |
| `RmDir(path)` | Must be **empty** (`KErrInUse` otherwise); ends with `\`; no wildcards |
| `Delete(file)` | One file, not open, not read-only/system; no wildcards |
| `Rename(old, new)` | Files or directories (no trailing `\`), same drive only; target must not exist; file must be closed. Moves within the drive without copying data. |
| `Replace(old, new)` | Move a file over another (overwrite); copies the target's attributes/times; both closed |
| `Att(name, TUint& att)` / `SetAtt(name, setMask, clearMask)` | `KEntryAttNormal`, `KEntryAttReadOnly`, `KEntryAttHidden`, `KEntryAttSystem`, `KEntryAttVolume`, `KEntryAttDir`, `KEntryAttArchive` |
| `Modified(name, TTime&)` / `SetModified(name, time)` | **UTC** times |
| `Entry(name, TEntry&)` / `SetEntry(name, time, set, clear)` | `TEntry`: `IsDir()`, `IsReadOnly()`, `IsHidden()`, `IsSystem()`, `IsArchive()`, `iAtt`, `iSize`, `iModified` (UTC), `iName` (leaf name), `iType` (UIDs; `entry[2]` = UID3), `IsTypeValid()`, `IsUidPresent()`, `MostDerivedUid()` |

```cpp
TUint att;
User::LeaveIfError(fs.Att(KFile, att));
TBool isArchive = (att & KEntryAttArchive) != 0;
fs.SetAtt(KFile, KEntryAttReadOnly, KEntryAttArchive);   // set RO, clear archive
```

Platform security: operations under `\sys\` or `\resource\` need `TCB` (writes) / `AllFiles` (reading `\sys\`); other apps' `\private\` need `AllFiles`. Without them you get `KErrPermissionDenied` — or the files simply appear not to exist.

## 18.6 Listing directories: `GetDir()` and `CDir`

```cpp
TInt GetDir(const TDesC& aName, TUint aEntryAttMask, TUint aEntrySortKey, CDir*& aEntryList);
TInt GetDir(const TDesC& aName, TUint aEntryAttMask, TUint aEntrySortKey, CDir*& aEntryList, CDir*& aDirList);
TInt GetDir(const TDesC& aName, const TUidType& aEntryUid, TUint aEntrySortKey, CDir*& aFileList);
```

* `aName`: a directory (`c:\dir\`) optionally with a wildcard (`c:\dir\*.jpg`).
* Attribute mask: `KEntryAttNormal` (files, not hidden/system/dirs), `KEntryAttMatchMask` (everything), `KEntryAttMaskSupported` (all except volumes), `KEntryAttDir` (include dirs), combined with `KEntryAttMatchExclude` (exclude those with the attributes) or `KEntryAttMatchExclusive` (only those), `KEntryAttAllowUid` (read UIDs).
* Sort key: `ESortNone` or one of `ESortByName` (default), `ESortByExt`, `ESortBySize`, `ESortByDate`, `ESortByUid`; plus `EAscending` (default)/`EDescending`/`EDirDescending`; plus `EDirsAnyOrder` (default)/`EDirsFirst`/`EDirsLast`.
* Filter by UID: `TUidType(KExecutableImageUid /*0x1000007A*/, KUidApp /*0x100039CE*/)`.
* `GetDir` **allocates** the `CDir`; you delete it. Don't pass an existing object.

```cpp
void ListDirL(RFs& aFs, const TDesC& aDir)     // aDir ends with '\'
    {
    CDir* entries = NULL;
    User::LeaveIfError(aFs.GetDir(aDir, KEntryAttMaskSupported, ESortByName | EDirsFirst, entries));
    CleanupStack::PushL(entries);
    for (TInt i = 0; i < entries->Count(); ++i)
        {
        const TEntry& e = (*entries)[i];
        if (e.IsDir())
            {
            HBufC* sub = HBufC::NewLC(aDir.Length() + e.iName.Length() + 1);
            TPtr p(sub->Des());
            p.Append(aDir);
            p.Append(e.iName);
            p.Append('\\');
            ListDirL(aFs, *sub);
            CleanupStack::PopAndDestroy(sub);
            }
        }
    CleanupStack::PopAndDestroy(entries);
    }
```

`CDirScan` walks a directory tree for you (`SetScanDataL(path, attMask, sortKey, CDirScan::EScanDownTree)`, then `NextL(CDir*&)`). `TFindFile` searches for a file across drives (`FindByDir(name, dir)`, `FindWildByDir()`, `Find()` for the next drive).

## 18.7 Other `RFs` functions

| Function | Purpose |
|----------|---------|
| `IsFileOpen(name, TBool&)`, `IsFileInRom(name)` | Status |
| `ReadFileSection(name, pos, TDes8&, len)` | Read without opening (works on open files; data may be stale) |
| `NotifyChange(TNotifyType, TRequestStatus& [, path])` / `NotifyChangeCancel()` | Async notification of changes (entries, files, attributes, disk). Path-specific watching of other apps' private dirs needs `AllFiles`. |
| `NotifyDiskSpace(threshold, drive, status)` | Free space crossing a threshold |
| `NotifyDismount(drive, status)` | Media being removed |
| `Subst()`, `ScanDrive()`, `SetDriveName()`, `SetVolumeLabel()`, `LockDrive()`… | Admin operations — most need `DiskAdmin` |
| `ShareProtected()` | Allow passing file handles to another process (`RFile::TransferToServer/AdoptFromClient`) |

## 18.8 `CFileMan` — wildcards, recursion, copy

`RFs` works on single objects, no copy, no recursion. `CFileMan` (same header/lib) adds that:

```cpp
CFileMan* fm = CFileMan::NewL(fs);            // or NewL(fs, observer)
CleanupStack::PushL(fm);

fm->Copy(_L("c:\\data\\"), _L("e:\\backup\\"), CFileMan::EOverWrite | CFileMan::ERecurse);
fm->Move(_L("c:\\*.txt"), _L("e:\\"), CFileMan::EOverWrite | CFileMan::ERecurse);
fm->Rename(_L("c:\\*.txt"), _L("c:\\*.bak"));             // wildcard rename, same drive, not recursive
fm->Delete(_L("c:\\temp\\*.tmp"), CFileMan::ERecurse);   // no read-only/open files
fm->RmDir(_L("c:\\temp\\"));                              // recursive, deletes everything below
fm->Attribs(_L("c:\\*.txt"), 0, KEntryAttHidden, TTime(0), CFileMan::ERecurse);

CleanupStack::PopAndDestroy(fm);
```

* Switches: `CFileMan::EOverWrite` (default for Copy/Move/Rename), `CFileMan::ERecurse`; pass `0` to forbid overwriting.
* `Copy` works across drives, copies attributes, and can read files opened with `EFileShareReadersOnly`; `Copy(RFile&, target)` copies an open file.
* `Rename(path\, dest\)` semantics: a trailing `\` means `\*.*`; `Rename(_L("c:\\src"), _L("c:\\trg\\"))` moves the directory *into* `trg`.
* All have **asynchronous overloads** (`TRequestStatus&` last) — `CFileMan` then uses an internal thread.
* **Observer** (`MFileManObserver`): `NotifyFileManStarted()`, `NotifyFileManOperation()` (each copy chunk), `NotifyFileManEnded()` return `EContinue`, `ERetry`, `ECancel` (skip this file) or `EAbort` (stop; the call returns `KErrCancel`). Inside, query `CurrentAction()` (`ECopy`, `EMove`, `EDelete`, `ERename`, `ERmDir`, `EAttribs`…), `CurrentEntry()`, `GetCurrentSource()`, `GetCurrentTarget()`, `FullPath()`, `AbbreviatedPath()`, `BytesTransferredByCopyStep()` (for progress bars), `GetLastError()`, `GetMoreInfoAboutError()` (`ESrcOpenFailed`, `ETrgOpenFailed`, `ENoFilesProcessed`…).

## 18.9 `RFile`

A **sub-session** of `RFs`: closing the `RFs` closes all its files.

### Opening

| Method | Behaviour |
|--------|-----------|
| `Open(fs, name, mode)` | Existing file |
| `Create(fs, name, mode)` | New file (`KErrAlreadyExists` if it exists); sets archive attribute |
| `Replace(fs, name, mode)` | Create, or truncate an existing file to 0 |
| `Temp(fs, dir, TFileName& name, mode)` | New uniquely named file in `dir`; you delete it afterwards |

```cpp
RFile file;
TInt err = file.Create(fs, KFileName, EFileWrite);
if (err == KErrAlreadyExists)
    err = file.Open(fs, KFileName, EFileWrite);
User::LeaveIfError(err);
CleanupClosePushL(file);
// ...
CleanupStack::PopAndDestroy(&file);
```

### Modes (`TFileMode`, OR one access mode with one share mode)

| Access | |
|--------|-|
| `EFileRead` (default) | Read only |
| `EFileWrite` | Read and write |

| Share | Others may… |
|-------|-------------|
| `EFileShareExclusive` (default) | not open the file at all |
| `EFileShareReadersOnly` | open it for reading only (can't be combined with `EFileWrite`) |
| `EFileShareAny` | open for read or write; later openers can't restrict to readers-only |
| `EFileShareReadersOrWriters` | open for read or write; may be restricted later |

Recommendations:
* Exclusive access: just `EFileRead` or `EFileWrite`.
* Shared file being written: writer `EFileShareAny | EFileWrite`; readers `EFileShareReadersOrWriters | EFileRead`.
* Shared read-only file: `EFileShareReadersOnly` (with `EFileRead`).
* Avoid `EFileShareReadersOrWriters | EFileWrite` and `EFileShareAny | EFileRead` (conflicts with readers-only openers).
* `ChangeMode()` switches between exclusive and readers-only on an open file.
* Other flags: `EFileStream`/`EFileStreamText` (legacy), `EFileWriteBuffered`, `EFileWriteDirectIO`, `EFileReadAheadOn/Off`.

### Reading and writing (8-bit descriptors only)

```cpp
TBuf8<15> buf;
file.Read(buf);            // up to MaxLength() bytes from the current position
file.Read(buf, 10);        // 10 bytes
file.Read(5, buf, 10);     // 10 bytes from offset 5
file.Read(5, buf);         // MaxLength() bytes from offset 5
// Reading past EOF is NOT an error: the descriptor just comes back shorter (length 0 at EOF).
```

* The target descriptor is overwritten (length reset). `KErrOverflow` if you ask for more than its max length.
* Async variants take a `TRequestStatus&`; cancel with `ReadCancel()`. Keep the buffer alive until completion.
* Read a whole file:

```cpp
TInt size = 0;
User::LeaveIfError(file.Size(size));
HBufC8* data = HBufC8::NewLC(size);
TPtr8 ptr(data->Des());
User::LeaveIfError(file.Read(0, ptr));
```

* Writing mirrors reading: `Write(des)`, `Write(des, len)`, `Write(pos, des)`, `Write(pos, des, len)` + async variants, `WriteCancel()`. The file grows automatically; to write from the middle of a descriptor pass `des.Mid(...)`.
* `Size()`, `SetSize()` (truncate/extend), `Flush()` (rarely needed; `Close()` flushes), `Lock(pos, len)` / `UnLock(pos, len)` (byte-range lock between clients), `Att()`, `SetAtt()`, `Modified()`, `SetModified()`, `Rename()`, `FullName()`, `Name()`, `Drive()`, `Duplicate()`.

Binary values and structs: wrap them in `TPckg` (see [Descriptors](09-descriptors.md#910-package-descriptors-tpckg-tpckgc-tpckgbuf)):

```cpp
TInt count = 3;
TPckg<TInt> countPkg(count);
file.Write(countPkg);
file.Read(0, countPkg);   // reads back into `count`
```

Variable-length records need your own framing (length prefixes) — or use **streams** ([Streams](19-streams-stores.md)), which are usually more convenient than raw `RFile`.

### Seeking

```cpp
TInt pos = 100;
file.Seek(ESeekStart, pos);     // absolute
pos = -50;
file.Seek(ESeekEnd, pos);       // from end (offset ≤ 0); pos receives the new position
pos = 0;
file.Seek(ESeekCurrent, pos);   // query current position without moving
```

`ESeekAddress` returns the address of a ROM (XIP) file's data. Seeks beyond the ends clamp to start/end.

### Text files: `TFileText`

Line-oriented 16-bit text I/O on an `RFile` (writes UTF-16 with CR-LF; reads CR, LF, CR-LF, LF-CR):

```cpp
TFileText text;
text.Set(file);
text.Write(_L("Line 1"));                 // appends CR-LF
text.Seek(ESeekStart);
TBuf<256> line;
while (text.Read(line) == KErrNone)       // KErrEof at the end; KErrTooBig if a line > 256 chars
    {
    // ...
    }
```

There's no 8-bit equivalent; for UTF-8 text read into an `HBufC8` and convert with `CnvUtfConverter`.

### Passing files between processes
`RFs::ShareProtected()` + `RFile::TransferToClient/TransferToServer/TransferToProcess` and `RFile::AdoptFromClient/AdoptFromServer/AdoptFromCreator` let a process holding access (e.g. to its private dir) hand an open file to another.

## 18.10 Performance tips
* One long-lived `RFs`; open files once, read/write in large chunks (each call is an IPC round trip).
* Prefer `RFileReadStream`/`RFileWriteStream` (buffered) for many small reads/writes.
* Avoid `TFileName` on the stack in recursive code.
* Check every returned error code; file operations on removable media fail often (`KErrNotReady`, `KErrDisMounted`, `KErrCorrupt`).

## See also
* [Streams and stores](19-streams-stores.md)
* [Platform security — data caging](02-platform-security.md#24-data-caging)

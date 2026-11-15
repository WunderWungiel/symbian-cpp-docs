# AGENTS.md — Guide for LLM Agents Writing Symbian C++

This file is for AI coding agents (and humans who want the short version). It lists the **non-negotiable rules**, the **defaults to choose**, **checklists**, and **where to look** in `docs/`. Symbian C++ is *not* standard C++: code that compiles but ignores these idioms will leak, panic, or fail to install.

## 0. Ground assumptions (state them if unsure)

* Target: **Symbian OS 9.x** (S60 3rd Edition = 9.1 and later, incl. 5th Edition, Symbian^3/Anna/Belle) unless the user says otherwise. Pre-9.x (S60 1st/2nd Ed., UIQ 2) is binary-incompatible and has no platform security.
* Build target for devices: **GCCE** (`abld build gcce urel`); emulator: **WINSCW UDEB**.
* No STL, no exceptions, no `std::string`, no RTTI-based designs in classic Symbian C++ (Open C/C++ exists, but don't mix it in unless asked).
* Signing today: **self-signed** → only user capabilities. Don't design features that need system/restricted capabilities unless the user controls the devices (see `docs/23-signing-certification.md`).

## 1. Hard rules

1. **Naming encodes semantics.** `T` value types (no destructor, no owned resources), `C` heap classes derived from `CBase`, `R` handles (must `Close()`), `M` interfaces (no data). Members `iX`, arguments `aX`, constants `KX`, enum values `EX`. → `docs/07-fundamentals.md`
2. **`L` suffix = may leave.** A function without `L` must never leave. `LC` = leaves an object on the cleanup stack. `D` = deletes itself.
3. **Never leave in a constructor or destructor.** Use two-phase construction: private C++ constructor + `ConstructL()`, public `NewL()`/`NewLC()`.
4. **Allocate with `new (ELeave)`**, never plain `new` unless you explicitly check for NULL.
5. **Every locally owned heap object or open R-handle must be on the cleanup stack across any leaving call.** `CleanupStack::PushL(ptr)`, `CleanupClosePushL(handle)`, `CleanupResetAndDestroyPushL(array)`. Pop with the pointer form: `CleanupStack::PopAndDestroy(ptr)`.
6. **Never push member variables** onto the cleanup stack. The owner's destructor frees them.
7. **Balance the cleanup stack inside each TRAP.** An `LC` function can't be the body of a TRAP.
8. **Descriptors:** pass as `const TDesC&` (in) and `TDes&` (out). Never pass `TDesC` by value. Use `_LIT`, not `_L`, in production. Check `MaxLength()` before writing (overflow = `USER 11` panic). Use `RBuf`/`HBufC` for large or unknown sizes. `%S` in `Format` takes `&descriptor`. → `docs/09-descriptors.md`
9. **Active objects:** add to the scheduler, one request at a time, `SetActive()` right after issuing, `Cancel()` first in the destructor, implement `DoCancel()`, keep async buffers as **members**. Never block the UI thread (`User::WaitForRequest`, `User::After`). → `docs/11-active-objects.md`
10. **Threads and console EXEs** must create a `CTrapCleanup` and (if they use AOs) a `CActiveScheduler`. GUI apps and servers get both from the framework.
11. **Platform security:** EXEs declare the **minimum** capabilities. Shared DLLs/plug-ins declare **generously** (`CAPABILITY ALL -TCB`), because a DLL can only be loaded by processes whose capabilities it covers. Store app data in the private directory (`RFs::PrivatePath()`). Never write to `\sys\` or `\resource\` at runtime. → `docs/02-platform-security.md`
12. **Binaries go to `\sys\bin\` and must have unique names**. Embed the UID: `MyApp_0xE1234567.exe`. Executables need unique UID3/SID. Use `0xE0000000–0xEFFFFFFF` for test builds.
13. **DLL exports are by ordinal.** `IMPORT_C` in headers, `EXPORT_C` in sources, then freeze (`abld freeze`) for **both** `bwins` and `eabi`. Never reorder existing exports. Avoid writable static data in DLLs. → `docs/13-dlls-libraries.md`
14. **Every client–server message must be completed exactly once.** Bad function codes panic the client. → `docs/15-client-server.md`
15. **ECom hosts must call `REComSession::FinalClose()`** at exit. Interface destructors call `DestroyedImplementation()`. → `docs/16-ecom.md`
16. **No leaks.** Debug builds panic `ALLOC` at exit. Delete everything you own, close every handle, `ResetAndDestroy()` pointer arrays.

## 2. Default choices

| Need | Use |
|------|-----|
| Small fixed string | `TBuf<N>` (N ≤ ~128) |
| Constant string | `_LIT(KName, "...")` |
| Large / dynamic string | `RBuf` (or `HBufC` + `Des()`) |
| View into existing text | `TPtrC` / `TPtr` |
| Binary data | `TBuf8`, `HBufC8`, `RBuf8`, `TPtrC8` |
| UTF-8 ↔ UTF-16 | `CnvUtfConverter` |
| Array of small values | `RArray<T>` |
| Array of owned C objects | `RPointerArray<T>` + `ResetAndDestroy()` |
| List of strings | `CDesCArrayFlat` (`bafl.lib`) |
| Async operation | Active object (`CActive`), not a thread |
| Periodic callback | `CPeriodic` / `CHeartbeat`; one-shot `CTimer`/`RTimer` |
| Background work in UI thread | `CIdle` or a self-completing AO |
| Persisting objects | `RFileWriteStream` + `ExternalizeL` |
| Settings | Central Repository, or `CDictionaryFileStore` in the private dir |
| IPC: request/response | Client–server |
| IPC: broadcast a value | Publish & Subscribe (`RProperty`) |
| Plug-ins | ECom |
| Network | `RSocketServ` + `RConnection` + `RSocket` (`RecvOneOrMore` for TCP); HTTP framework for HTTP |
| UI (S60) | Avkon: `CAknApplication`/`CAknDocument`/`CAknAppUi`(+`CAknView`)/`CCoeControl` |

## 3. Templates to reuse

* Two-phase C class: `docs/07-fundamentals.md#74-two-phase-construction`
* Console `E32Main` with cleanup stack + scheduler: `docs/12-applications.md#121-entry-point-and-the-minimal-console-program`
* Active object (timer): `docs/11-active-objects.md#113-writing-an-active-object`
* GUI app skeleton (entry, application, document, AppUi, container, resources): `docs/12-applications.md#123-s60-gui-application-architecture`
* Registration file (`_reg.rss`) and icon makefile: `docs/12-applications.md#124-registering-the-application-in-the-menu`
* Shared DLL / polymorphic DLL: `docs/13-dlls-libraries.md`
* Server + session + client: `docs/15-client-server.md`
* ECom interface + plug-in + registry resource: `docs/16-ecom.md`
* P&S watcher AO: `docs/14-processes-threads-ipc.md`
* PKG file: `docs/05-sdk-and-build.md#57-packaging-the-pkg-file`
* Buildable examples: `examples/`

## 4. Pre-commit checklist for generated code

- [ ] Every `L` function is only called from `L` functions or inside a TRAP.
- [ ] No leaving calls in constructors or destructors.
- [ ] Each `PushL` has a matching `Pop`/`PopAndDestroy` on all paths. The pointer form is used.
- [ ] All R-handles are closed; all `CActive`s call `Cancel()` in the destructor.
- [ ] Async buffers and `TRequestStatus` are members, not locals.
- [ ] Descriptor sizes are checked; no `TFileName` arrays on the stack.
- [ ] MMP: correct `TARGETTYPE`, `UID`, `CAPABILITY`, `LIBRARY` for every API used (e.g. `efsrv.lib` for `RFs`, `estor.lib` for streams, `bafl.lib` for `CDesCArray`, `ws32.lib`, `cone.lib`, `avkon.lib`, `ecom.lib`, `esock.lib`, `insock.lib`, `charconv.lib`, `edbms.lib`).
- [ ] GUI app: `EPOCSTACKSIZE 0x5000`, `_reg.rss` + localisable info + MIF icon in MMP and PKG.
- [ ] PKG: platform UID line, all binaries to `!:\sys\bin\`, resources to `!:\resource\apps\`, registration to `!:\private\10003a3f\import\apps\`.
- [ ] UID3/SID unique and consistent across MMP, `_reg.rss`, code constants, PKG and private paths.
- [ ] For DLLs: exports frozen, `.def` files updated for both ABIs.

## 5. Common mistakes LLMs make in Symbian code (avoid)

| Mistake | Correct |
|---------|---------|
| `std::string`, `std::vector`, `try/catch`, `throw` | Descriptors, `RArray`, leaves/TRAP |
| `bool`, `int`, `void*` | `TBool`, `TInt`, `TAny*` |
| `if (b == ETrue)` | `if (b)` |
| `new CFoo` then use | `CFoo::NewL()` / `new (ELeave)` |
| Calling `ConstructL()` from the constructor | Two-phase via `NewL` |
| `TDesC` by value / returning a reference to a local `TBuf` | `const TDesC&`; return `HBufC*` |
| `buf.Format(_L("%s"), des)` | `buf.Format(KFmt, &des)` with `%S` |
| `RFile` opened but never closed on the leave path | `CleanupClosePushL(file)` |
| Forgetting `SetActive()` / `Cancel()` | Stray signal (`E32USER-CBase 46`) / panic 40 |
| `User::WaitForRequest()` in GUI code | Active object |
| Hard-coded `C:\` for app data | `RFs::PrivatePath()` + the app's install drive |
| Declaring `AllFiles`/`TCB` "to be safe" | Minimum capabilities; those can't be granted to third parties |
| Inventing API names | Check the relevant `docs/` chapter or SDK headers; if unsure, say so |
| `CleanupStack::PushL(member)` | Members are freed in the destructor |
| Inheriting `M` before `CBase` | `class CFoo : public CBase, public MBar` |
| DLL with global non-const data | Move state into objects / TLS (`Dll::SetTls`) |

## 6. Where to look

* Build/packaging/install errors → `docs/05-sdk-and-build.md` (§5.9 troubleshooting)
* Panic codes → `docs/08-errors-leaves-cleanup.md` (§8.4)
* Capabilities per area → `docs/02-platform-security.md` (§2.2), per-chapter notes
* Key codes → `docs/22-window-server-graphics.md`
* Terms → `GLOSSARY.md`

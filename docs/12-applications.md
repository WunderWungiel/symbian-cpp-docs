# 12. Applications: Console, GUI, Registration, Icons, Autostart

> **Scope:** the minimal console EXE, the console API, the S60 GUI application architecture (Application / Document / AppUi / View / Container), registering an app in the menu, captions and icons, renaming/changing UIDs, and starting at boot.

## 12.1 Entry point and the minimal console program

Every EXE has `E32Main()` as its entry point. Carbide.c++'s *Basic console application* template generates this structure:

```cpp
// HelloWorld.cpp
#include <e32base.h>
#include <e32std.h>
#include <e32cons.h>          // Console, CConsoleBase

_LIT(KTextConsoleTitle, "Console");
_LIT(KTextFailed, " failed, leave code = %d");
_LIT(KTextPressAnyKey, " [press any key]\n");

LOCAL_D CConsoleBase* console;   // global, used by all functions

LOCAL_C void MainL()
    {
    _LIT(KHello, "Hello, world!\n");
    console->Write(KHello);       // your code goes here
    }

LOCAL_C void DoStartL()
    {
    CActiveScheduler* scheduler = new (ELeave) CActiveScheduler();
    CleanupStack::PushL(scheduler);
    CActiveScheduler::Install(scheduler);   // only needed if you use active objects
    MainL();
    CleanupStack::PopAndDestroy(scheduler);
    }

GLDEF_C TInt E32Main()
    {
    __UHEAP_MARK;
    CTrapCleanup* cleanup = CTrapCleanup::New();
    TRAPD(createError, console = Console::NewL(KTextConsoleTitle,
                                                TSize(KConsFullScreen, KConsFullScreen)));
    if (createError)
        return createError;
    TRAPD(mainError, DoStartL());
    if (mainError)
        console->Printf(KTextFailed, mainError);
    console->Printf(KTextPressAnyKey);
    console->Getch();
    delete console;
    delete cleanup;
    __UHEAP_MARKEND;
    return KErrNone;
    }
```

MMP: `TARGETTYPE exe`, `UID 0 0xE6205EE2`, `LIBRARY euser.lib`. **`euser.lib` is always needed.** `e32base.h` and `e32std.h` cover most basic classes.

Scope macros from `e32def.h` (purely documentary): `GLDEF_C`/`GLREF_C` (global function definition/declaration), `LOCAL_C` (`static` function), `GLDEF_D`/`GLREF_D` (global data / `extern`), `LOCAL_D` (`static` data).

A console EXE installed as-is has **no menu icon** — add a registration file (§12.4) or start it from another app.

### Console API (`CConsoleBase`, via `Console::NewL(title, size)`)
* The concrete console is device-specific; the size argument is ignored on devices (always full screen).
* `Write(des)`, `Printf(fmt, ...)`, `ClearScreen()`, `ClearToEndOfLine()`, `SetPos(x, y)`, `CursorPos()`, `WhereX()`, `WhereY()`, `ScreenSize()`.
* Input: `Getch()` (synchronous key read), `Read(TRequestStatus&)` + `KeyCode()`, `KeyModifiers()` (async, for an active object), `ReadCancel()`.
* Good enough for test harnesses and pseudo-graphics; not for real UIs.

## 12.2 Background EXEs (no UI)

A background "daemon" or service is just an EXE with no console: create a cleanup stack and an active scheduler in `E32Main()`, create your AOs, `CActiveScheduler::Start()`. Don't build daemons from the GUI template — the UI framework wastes memory and battery. Use `RProcess::Rendezvous()` to tell a launcher you're ready, and a single-instance check (e.g. a named mutex or `TFindProcess`) to avoid duplicates.

## 12.3 S60 GUI application architecture

A GUI app is an EXE that hands control to the framework. The classes (Avkon names; Uikon/Eikon bases):

| Class | Base | Role |
|-------|------|------|
| Application | `CAknApplication` (`CEikApplication`) | Factory: returns the app UID and creates the document. |
| Document | `CAknDocument` (`CEikDocument`) | Owns the data model ("engine"); creates the AppUi. File-based apps load/save here. |
| AppUi | `CAknAppUi` or `CAknViewAppUi` | Event handling hub: commands (`HandleCommandL`), keys, foreground/background, layout changes; owns views/containers. |
| View (optional) | `CAknView` | One screen in a multi-view app; registered with the View Server; has its own menu/CBA resources (`AVKON_VIEW`). |
| Container / control | `CCoeControl` | Draws and handles keys/pointer for (part of) the screen. |

Typical files from the Carbide.c++ *S60 GUI application* template: `MyApp.cpp` (entry), `MyAppApplication.cpp`, `MyAppDocument.cpp`, `MyAppAppUi.cpp`, `MyAppAppView.cpp` (container), `MyApp.hrh` (command IDs), `MyApp.pan`, `data/MyApp.rss`, `data/MyApp_reg.rss`, `data/MyApp.loc`/`.l01`, `gfx/*.svg`, `group/Icons_scalable_dc.mk`.

### Entry point

```cpp
// MyApp.cpp
#include <eikstart.h>
#include "MyAppApplication.h"

LOCAL_C CApaApplication* NewApplication()
    {
    return new CMyAppApplication;        // plain new: NULL on failure is handled by the framework
    }

GLDEF_C TInt E32Main()
    {
    return EikStart::RunApplication(NewApplication);
    }
```

The framework creates the cleanup stack, active scheduler, `CEikonEnv`/`CAknEnv`, loads the main resource file, then calls your factories.

### Application and document

```cpp
const TUid KUidMyApp = { 0xE1234567 };   // == UID3 in the MMP

class CMyAppApplication : public CAknApplication
    {
public:
    TUid AppDllUid() const { return KUidMyApp; }
protected:
    CApaDocument* CreateDocumentL() { return CMyAppDocument::NewL(*this); }
    };

class CMyAppDocument : public CAknDocument
    {
public:
    static CMyAppDocument* NewL(CEikApplication& aApp);
    CEikAppUi* CreateAppUiL() { return new (ELeave) CMyAppAppUi; }   // framework calls ConstructL
private:
    CMyAppDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
    };
```

### AppUi

```cpp
class CMyAppAppUi : public CAknAppUi
    {
public:
    void ConstructL();
    ~CMyAppAppUi();
private:
    void HandleCommandL(TInt aCommand);          // menu/softkey commands
    void HandleResourceChangeL(TInt aType);       // layout/skin change (screen rotation)
    // TKeyResponse HandleKeyEventL(const TKeyEvent&, TEventCode);  // optional
private:
    CMyAppAppView* iAppView;
    };

void CMyAppAppUi::ConstructL()
    {
    BaseConstructL(EAknEnableSkin);               // S60 3rd+: enable skins (and EAknTouchCompatible on 5th Ed.)
    iAppView = CMyAppAppView::NewL(ClientRect());
    AddToStackL(iAppView);                        // receive key events (if not using views)
    }

CMyAppAppUi::~CMyAppAppUi()
    {
    if (iAppView)
        {
        RemoveFromStack(iAppView);
        delete iAppView;
        }
    }

void CMyAppAppUi::HandleCommandL(TInt aCommand)
    {
    switch (aCommand)
        {
        case EEikCmdExit:
        case EAknSoftkeyExit:
            Exit();
            break;
        case EMyAppCmdHello:
            {
            _LIT(KMsg, "Hello!");
            CAknInformationNote* note = new (ELeave) CAknInformationNote;
            note->ExecuteLD(KMsg);
            }
            break;
        default:
            break;
        }
    }
```

For multi-view apps derive from `CAknViewAppUi`, create `CAknView` subclasses, `AddViewL(view)`, `SetDefaultViewL(*view)`, and switch with `ActivateLocalViewL(KViewId)`. Each view implements `Id()`, `DoActivateL()` (create its container) and `DoDeactivate()` (destroy it), and `HandleCommandL()`.

### Container (control)

```cpp
class CMyAppAppView : public CCoeControl
    {
public:
    static CMyAppAppView* NewL(const TRect& aRect);
    void Draw(const TRect& aRect) const;
    void SizeChanged();
    TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
private:
    void ConstructL(const TRect& aRect);
    };

void CMyAppAppView::ConstructL(const TRect& aRect)
    {
    CreateWindowL();          // window-owning control
    SetRect(aRect);
    ActivateL();
    }

void CMyAppAppView::Draw(const TRect& /*aRect*/) const
    {
    CWindowGc& gc = SystemGc();
    gc.Clear(Rect());
    const CFont* font = CEikonEnv::Static()->NormalFont();
    gc.UseFont(font);
    _LIT(KText, "Hello World");
    gc.DrawText(KText, TPoint(10, 40));
    gc.DiscardFont();
    }
```

Compound controls override `CountComponentControls()` and `ComponentControl(TInt)`; call `DrawNow()`/`DrawDeferred()` to redraw. See [Window server](22-window-server-graphics.md).

### Main resource file

```c
// MyApp.rss
NAME MYAP

#include <eikon.rh>
#include <avkon.rh>
#include <avkon.rsg>
#include <appinfo.rh>
#include "MyApp.hrh"
#include "MyApp.loc"

RESOURCE RSS_SIGNATURE { }
RESOURCE TBUF { buf = "MyApp"; }

RESOURCE EIK_APP_INFO
    {
    menubar = r_myapp_menubar;
    cba     = R_AVKON_SOFTKEYS_OPTIONS_EXIT;
    }

RESOURCE MENU_BAR r_myapp_menubar
    {
    titles = { MENU_TITLE { menu_pane = r_myapp_menu; } };
    }

RESOURCE MENU_PANE r_myapp_menu
    {
    items =
        {
        MENU_ITEM { command = EMyAppCmdHello; txt = STR_myapp_hello; },
        MENU_ITEM { command = EAknCmdExit;    txt = STR_myapp_exit; }
        };
    }

RESOURCE LOCALISABLE_APP_INFO r_myapp_localisable_app_info
    {
    short_caption = STR_myapp_short_caption;
    caption_and_icon = CAPTION_AND_ICON_INFO
        {
        caption         = STR_myapp_caption;
        number_of_icons = 1;
        icon_file       = "\\resource\\apps\\MyApp_0xE1234567.mif";
        };
    }
```

```c
// MyApp.hrh
enum TMyAppCommands { EMyAppCmdHello = 0x6001 };
```

Typical GUI libraries: `euser.lib apparc.lib cone.lib eikcore.lib avkon.lib commonengine.lib efsrv.lib estor.lib aknnotify.lib eikcoctl.lib eikctl.lib gdi.lib ws32.lib bafl.lib aknicon.lib aknskins.lib`. GUI apps need `EPOCSTACKSIZE 0x5000`.

### Common S60 UI building blocks (Avkon)
* Notes: `CAknInformationNote`, `CAknConfirmationNote`, `CAknWarningNote`, `CAknErrorNote` (`ExecuteLD(text)`); global notes `CAknGlobalNote`.
* Queries: `CAknQueryDialog` (yes/no), `CAknTextQueryDialog`, `CAknNumberQueryDialog`, `CAknMultiLineDataQueryDialog`, `CAknListQueryDialog`.
* Lists: `CAknSingleStyleListBox`, `CAknDoubleStyleListBox`, `CAknSingleGraphicStyleListBox`, with `CDesCArray` item text (`"\tItem"`), `CAknColumnListBox`; settings lists `CAknSettingItemList`; forms `CAknForm`.
* Status pane: title (`CAknTitlePane`), navi pane, context pane via `StatusPane()->ControlL(TUid::Uid(EEikStatusPaneUidTitle))`.
* Wait/progress: `CAknWaitDialog`, `CAknProgressDialog`.
* Skins: `MAknsSkinInstance`, `AknsDrawUtils::Background()`, `CAknsBasicBackgroundControlContext`.
* Layout: `AknLayoutUtils::LayoutMetricsRect()`; handle `KEikDynamicLayoutVariantSwitch` in `HandleResourceChangeL` for rotation.
* Touch (5th Ed.+): `HandlePointerEventL()`, `AknLayoutUtils::PenEnabled()`, `EAknTouchCompatible` flag.

## 12.4 Registering the application in the menu

The menu (AppArc, process `apsexe.exe`, SID `0x10003A3F`) learns about apps from **registration files** placed into `\private\10003a3f\import\apps\` **at install time** (only the installer can write there).

```c
// MyApp_reg.rss
#include <appinfo.rh>
#include <MyApp_0xE1234567.rsg>          // for R_MYAPP_LOCALISABLE_APP_INFO

UID2 KUidAppRegistrationResourceFile
UID3 0xE1234567                          // == UID3 of the EXE

RESOURCE APP_REGISTRATION_INFO           // must be the first resource
    {
    app_file = "MyApp_0xE1234567";       // EXE name without .exe
    localisable_resource_file = "\\resource\\apps\\MyApp_0xE1234567";   // no extension (localized variant chosen)
    localisable_resource_id   = R_MYAPP_LOCALISABLE_APP_INFO;
    // hidden = KAppHidden;              // don't show in menu (default KAppNotHidden)
    // launch = KAppLaunchInBackground;  // default KAppLaunchInForeground
    // embeddability = KAppNotEmbeddable;
    // datatype_list = { DATATYPE { priority = EDataTypePriorityNormal; type = "text/x-myformat"; } };
    }
```

* `UID2` **must** be `KUidAppRegistrationResourceFile` (else AppArc ignores the file); `UID3` must equal the EXE's UID3.
* `LOCALISABLE_APP_INFO` **cannot** be in the registration file — it lives in the main (or a separate localizable) resource file. That file must be listed **before** the `_reg.rss` in the MMP so its `.rsg` exists.
* `datatype_list` registers MIME types the app handles (opened via *File → Open* in the emulator).

MMP and PKG:

```text
SOURCEPATH ..\data
START RESOURCE MyApp_0xE1234567.rss
HEADER
TARGETPATH \resource\apps
END
START RESOURCE MyApp_0xE1234567_reg.rss
TARGETPATH \private\10003a3f\apps        // emulator location (\epoc32\data\z\private\10003a3f\apps)
END
```

```text
"$(EPOCROOT)Epoc32\data\z\resource\apps\MyApp_0xE1234567.rsc" - "!:\resource\apps\MyApp_0xE1234567.rsc"
"$(EPOCROOT)Epoc32\data\z\private\10003a3f\apps\MyApp_0xE1234567_reg.rsc" - "!:\private\10003a3f\import\apps\MyApp_0xE1234567_reg.rsc"
```

Note the different paths: `\private\10003a3f\apps` (emulator/ROM) vs `\private\10003a3f\import\apps` (installed).

Even a **console EXE** can get a menu icon this way: give it a `_reg.rss` (and optionally a `LOCALISABLE_APP_INFO` resource for the caption).

## 12.5 Icons (SVG-T → MIF)

* S60 3rd Edition+ app icons are **SVG-Tiny** images packed into a **`.mif`** (multi-image file) with `mifconv`. Bitmaps (`.bmp` → `.mbm`) are still possible for in-app graphics.
* SVG-T forbids embedded raster images. Many converters merely embed a PNG (`<image>` with base64 data) — such icons **don't display**. Draw real vectors (e.g. Inkscape, save as plain SVG) and check with `svg2svgt` from the SDK.
* `mifconv` encodes SVGs to binary with `svgtbinencode`. **Binary-encoded SVG isn't supported on Symbian 9.1**, and S60 3rd FP1+ SDKs encode by default → icons built with newer SDKs may not appear on 3rd Edition MR phones. Pass **`/X`** to `mifconv` to disable encoding.
* Icons are cached: restart the emulator/phone (or reinstall) to see changes.

Extension makefile (`group/MyApp_icons.mk`, listed in `bld.inf` as `gnumakefile MyApp_icons.mk` **before** the MMP):

```make
ifeq (WINS,$(findstring WINS, $(PLATFORM)))
ZDIR=$(EPOCROOT)epoc32\release\$(PLATFORM)\$(CFG)\Z
else
ZDIR=$(EPOCROOT)epoc32\data\z
endif

TARGETDIR=$(ZDIR)\resource\apps
ICONTARGETFILENAME=$(TARGETDIR)\MyApp_0xE1234567.mif
ICONDIR=..\gfx

do_nothing :
	@rem do_nothing

MAKMAKE : do_nothing
BLD : do_nothing
CLEAN : do_nothing
LIB : do_nothing
CLEANLIB : do_nothing

RESOURCE : $(ICONTARGETFILENAME)

$(ICONTARGETFILENAME) : $(ICONDIR)\qgn_menu_myapp.svg
	mifconv $(ICONTARGETFILENAME) /X /c32 $(ICONDIR)\qgn_menu_myapp.svg

FREEZE : do_nothing
SAVESPACE : do_nothing
RELEASABLES :
	@echo $(ICONTARGETFILENAME)
FINAL : do_nothing
```

(Recipe lines must start with a **tab**.) `mifconv` options: `/c<depth>[,<maskdepth>]` before each file (e.g. `/c32,8` for 32-bit colour + 8-bit mask, `/c24,1`), `/H<file>.mbg` to generate an index header, `/X` disable SVG binary encoding. After adding the `.mk` to `bld.inf`, run `bldmake bldfiles` again.

Then reference the MIF in `CAPTION_AND_ICON_INFO` (`icon_file`, `number_of_icons = 1`) and install it to `!:\resource\apps\`.

Loading icons in code: `AknIconUtils::CreateIconL(bitmap, mask, KMifFile, EMbmMyappIcon, EMbmMyappIcon_mask)` then `AknIconUtils::SetSize(bitmap, size)` — SVG icons have no size until you set one.

## 12.6 Naming executables and changing UIDs

* Executables share `\sys\bin\` → **name clash** = install failure. Include the UID: `HelloWorld_0xE6205EE2.exe`.
* UID3 (= SID) must be unique on the device. Clashes typically happen when developers install an SDK example and later reuse it as a template without changing the UID.
* Decide the signing route early: self-signed → unprotected UID (`0xE…`/`0xA…`); Symbian Signed → protected UID (`0x2…`). Discovering late that you need e.g. `NetworkControl` forces a UID change.
* Changing UID3 / the EXE name means touching **all** of: MMP (`TARGET`, `UID`, `SECUREID`), `_reg.rss` (`UID3`, `app_file`), the main `.rss` name and `TARGET` of resources, PKG (PID, file names, `\private\<SID>\` paths), the `KUidMyApp` constant in code, `.uidesign` files, help files, and any other component referring to it. Search the whole project (and dependent projects), rebuild and retest.

## 12.7 Autostart at boot (Startup List Management)

From 9.x, the **Startup List Management (SLM) API** starts registered EXEs at boot. Register at install time by placing a resource in the import directory of the installer/SLM process (SID `0x101F875A`):

```c
// MyApp_startup.rss  (file name can't contain [ ] — rename in the PKG)
#include <startupitem.rh>

RESOURCE STARTUP_ITEM_INFO
    {
    executable_name = "!:\\sys\\bin\\MyApp_0xE1234567.exe";   // '!' = search drives
    recovery = EStartupItemPolicyNone;                        // the only value; default
    }
```

```text
SOURCEPATH ..\data
START RESOURCE MyApp_startup.rss
END
```

```text
; installed name MUST be [<PID>].rsc, where PID is the package UID from the PKG header
"$(EPOCROOT)Epoc32\data\MyApp_startup.rsc" - "!:\private\101f875a\import\[E1234567].rsc"
```

Conditions:
* The package must be type `SA` and **signed with a trusted certificate** (DevCert/Open Signed/Symbian Signed). **Self-signed packages' startup files are ignored.**
* The started EXE must be in the same package. Multiple `STARTUP_ITEM_INFO` entries allowed; no other resource types in the file.
* SLM doesn't work in the emulator.
* Before 9.x (and as a hack on 9.x) apps autostarted via a **recognizer** plug-in loaded at boot (see [ECom](16-ecom.md#167-recognizers)).
* Good practice: give the user a setting to disable autostart (e.g. check a flag file at start and exit immediately).

## See also
* [Resource files](04-resource-files.md)
* [Packaging](05-sdk-and-build.md#57-packaging-the-pkg-file)
* [Window server and graphics](22-window-server-graphics.md)

# 22. The Window Server, Key Events and Graphics

> **Scope:** the Window Server (WSERV) and its window tree, sessions and window groups, key events and global key capture, raw windows and redraws, graphics contexts and drawing, fonts, bitmaps (`CFbsBitmap`, off-screen drawing), and pointers to DSA, sprites and anims.
> **Headers / libs:** `w32std.h` (`ws32.lib`), `gdi.h` (`gdi.lib`), `fbs.h` (`fbscli.lib`), `bitstd.h`/`bitdev.h` (`bitgdi.lib`). In GUI apps CONE wraps most of this (controls, `SystemGc()`), but understanding WSERV explains how controls work and lets EXEs without a UI framework draw and capture keys.

## 22.1 Window Server basics

* **WSERV** (`wserv.exe`, SID `0x10003B20`) starts at boot and owns the **screen** and **input** (keys, pointer). It decides which windows are visible, routes events, renders drawing commands and handles screen orientation.
* Windows form a **tree**:

```
Root window (one per screen, created by WSERV)
└── Window groups (RWindowGroup) — one or more per application; receive key focus; not drawable
    └── Windows: RBlankWindow, RWindow, RBackedUpWindow (and child windows inside them)
```

* Class hierarchy: `RWindowTreeNode` (tree navigation, ordinal positions) → `RWindowGroup`, `RWindowBase` (position, size) → `RBlankWindow` (no drawing), `RDrawableWindow` (base for drawable) → `RWindow` (normal; app redraws on request) and `RBackedUpWindow` (server keeps a backup bitmap — faster redraws, more memory).
* Z-order: a child is drawn over its parent and clipped to it; among siblings the "older"/lower ordinal position is on top. Window groups also have **priorities**; ordinal position orders groups of equal priority. Position `-1` sends a group to the back.
* Coordinates are relative to the parent window (to the screen for windows directly under a group).

## 22.2 Session and window group

```cpp
RWsSession ws;
User::LeaveIfError(ws.Connect());            // or Connect(RFs&) to share a file session
CleanupClosePushL(ws);

RWindowGroup wg(ws);
User::LeaveIfError(wg.Construct(reinterpret_cast<TUint32>(&wg)));   // client handle: any unique value
CleanupClosePushL(wg);
// wg.Construct(handle, EFalse)  → a group that never takes keyboard focus
// wg.ConstructChildApp(parentId, handle) → chain to another app's group (embedding)
```

A new group starts at ordinal position 0 (front) and, if focusable, gets keyboard focus. Useful members: `SetOrdinalPosition(pos [, priority])`, `OrdinalPosition()`, `OrdinalPriority()`, `EnableReceiptOfFocus(TBool)`, `SetName()`, `Identifier()`, `AutoForeground()`, `EnableGroupListChangeEvents()`, `EnableFocusChangeEvents()`, `EnableScreenChangeEvents()`. In a GUI app, use `CCoeEnv::Static()->WsSession()` and `RootWin()` instead of creating your own.

## 22.3 Receiving events

Standard events (keys, pointer, focus, group changes…) are delivered via `EventReady()` / `GetEvent()`; redraws via `RedrawReady()` / `GetRedraw()`; priority keys via `PriorityKeyReady()`. Each is an async request — normally run by an active object:

```cpp
void CEventWatcher::Start()
    {
    iWs.EventReady(&iStatus);
    SetActive();
    }

void CEventWatcher::RunL()
    {
    if (iStatus != KErrNone) return;
    TWsEvent e;
    iWs.GetEvent(e);              // must be called before the next EventReady()
    switch (e.Type())
        {
        case EEventKey:      HandleKey(e.Key()->iCode, e.Key()->iModifiers); break;
        case EEventKeyDown:  HandleDown(e.Key()->iScanCode); break;
        case EEventKeyUp:    break;
        case EEventPointer:  HandlePointer(*e.Pointer()); break;
        case EEventFocusGained:
        case EEventFocusLost: break;
        default: break;
        }
    Start();
    }

void CEventWatcher::DoCancel() { iWs.EventReadyCancel(); }
```

* `TWsEvent`: `Type()` (`TEventCode`), `Handle()` (which window/group), `Time()`, `Key()` → `TKeyEvent*`, `Pointer()` → `TPointerEvent*`, `ModifiersChanged()`.
* Common types: `EEventNull` (ignore), `EEventKeyDown`, `EEventKey`, `EEventKeyUp`, `EEventModifiersChanged`, `EEventPointer`, `EEventPointerEnter/Exit`, `EEventDragDrop`, `EEventFocusGained`, `EEventFocusLost`, `EEventFocusGroupChanged`, `EEventWindowGroupsChanged`, `EEventWindowGroupListChanged` (enable first), `EEventWindowVisibilityChanged`, `EEventScreenDeviceChanged` (rotation), `EEventSwitchOn`.
* Order for a typical key press: `EEventKeyDown` → `EEventKey` (possibly repeated) → `EEventKeyUp`.

### `TKeyEvent`

| Field | Meaning |
|-------|---------|
| `iCode` | Character/key code (`TKeyCode`, `e32keys.h`) — only in `EEventKey` |
| `iScanCode` | Physical key (`TStdScanCode`) — in down/up events |
| `iModifiers` | `EModifierShift`, `EModifierCtrl`, `EModifierFunc`, `EModifierAutorepeatable`… |
| `iRepeats` | Auto-repeat count |

Nokia S60 conventions (not standardised — vendors deviate; check on hardware):

| Key | `iCode` | `iScanCode` |
|-----|---------|-------------|
| Left softkey | `EKeyDevice0` | `EStdKeyDevice0` |
| Right softkey | `EKeyDevice1` | `EStdKeyDevice1` |
| Middle softkey / select | `EKeyDevice3` (also `EKeyOK`) | `EStdKeyDevice3` |
| Power key | `EKeyDevice2` | |
| Call (green) / End (red) | `EKeyYes` / `EKeyNo` | `EStdKeyYes` / `EStdKeyNo` |
| Menu (Applications) key | `EKeyApplication0` | `EStdKeyApplication0` |
| Navigation | `EKeyUpArrow`, `EKeyDownArrow`, `EKeyLeftArrow`, `EKeyRightArrow` | `EStdKeyUpArrow`… |
| Clear | `EKeyBackspace` | `EStdKeyBackspace` |
| Edit (pencil) | — | `EStdKeyLeftShift` / `EStdKeyRightShift` |
| Camera, media keys | device-specific; often need the **Remote Control framework** (`CRemConInterfaceSelector`, `MRemConCoreApiTargetObserver`) or vendor APIs | |

In GUI apps you get keys via `CCoeControl::OfferKeyEventL(const TKeyEvent&, TEventCode)` (return `EKeyWasConsumed`/`EKeyWasNotConsumed`) on controls in the AppUi's control stack, and `CAknAppUi::HandleKeyEventL()`.

### Capturing keys globally (background apps)

`RWindowGroup::CaptureKey(code, modMask, modifiers)` (`EEventKey`), `CaptureKeyUpAndDowns(scanCode, ...)` (down/up), `CaptureLongKey(...)` deliver the key to **your group even without focus**. They return a handle (or error) used to cancel: `CancelCaptureKey()`, `CancelCaptureKeyUpAndDowns()`, `CancelCaptureLongKey()`. **Requires `SwEvent`** (for keys other apps would get).

Background key-catcher pattern:

```cpp
void CKeyCatcher::ConstructL()
    {
    User::LeaveIfError(iWs.Connect());
    iWg = new (ELeave) RWindowGroup(iWs);                 // heap: needs the connected session
    User::LeaveIfError(iWg->Construct(reinterpret_cast<TUint32>(iWg), EFalse));   // never takes focus
    iWg->SetOrdinalPosition(-1);                          // send to back
    iWg->EnableReceiptOfFocus(EFalse);
    CApaWindowGroupName* name = CApaWindowGroupName::NewLC(iWs);
    name->SetHidden(ETrue);                               // hide from the task list
    name->SetWindowGroupName(*iWg);
    CleanupStack::PopAndDestroy(name);
    CActiveScheduler::Add(this);
    }

TInt CKeyCatcher::CaptureL(TUint aKeyCode)
    {
    TInt32 handle = iWg->CaptureKey(aKeyCode, 0, 0);
    User::LeaveIfError(handle);
    iHandles.AppendL(handle);
    return KErrNone;
    }

void CKeyCatcher::RunL()
    {
    if (iStatus != KErrNone) return;
    TWsEvent e;
    iWs.GetEvent(e);
    if (e.Type() == EEventKey)
        {
        TBool consume = iObserver.HandleKeyL(e.Key()->iCode);
        if (!consume)                                     // pass it on to whoever has focus
            iWs.SendEventToWindowGroup(iWs.GetFocusWindowGroup(), e);
        }
    Start();                                              // EventReady() again
    }

CKeyCatcher::~CKeyCatcher()
    {
    Cancel();
    for (TInt i = 0; i < iHandles.Count(); ++i)
        iWg->CancelCaptureKey(iHandles[i]);
    iHandles.Close();
    if (iWg) { iWg->Close(); delete iWg; }
    iWs.Close();
    }
```

Caveats: a captured key is delivered **only** to the capturer — always forward keys you don't consume (`SendEventToWindowGroup()`, `SendEventToAllWindowGroups()`, `SendEventToOneWindowGroupsPerClient()`), otherwise the foreground app stops receiving them. If two apps capture the same key, only the **last** one gets it. Some apps (certain Java MIDlets) don't accept forwarded events. Helpers: `GetFocusWindowGroup()`, `NumWindowGroups()`, `WindowGroupList()`, `FindWindowGroupIdentifier()`, `CApaWindowGroupName`, `TApaTaskList`.

A more robust (but harder) interception route is a **window-server anim plug-in** (`CAnim`/`CWindowAnim` DLL loaded into WSERV) which sees `TRawEvent`s before WSERV processes them.

## 22.4 Windows and redraws

```cpp
iWindow = new (ELeave) RWindow(iWs);
User::LeaveIfError(iWindow->Construct(*iWg, reinterpret_cast<TUint32>(iWindow)));
iWindow->SetExtent(TPoint(10, 10), TSize(100, 100));   // or SetPosition()/SetSize()
iWindow->SetBackgroundColor(KRgbWhite);                 // used for server-initiated clears
iWindow->SetShadowHeight(1);
iWindow->Activate();                                    // visible & receives redraw events from now on
```

Other properties: `SetRequiredDisplayMode()` (`EColor64K`, `EColor16M`, `EColor16MU`…), `SetShape()`, `SetCornerType()`, `SetVisible()`, `SetOrdinalPosition()`, `EnableRedrawStore()`, `SetTransparencyAlphaChannel()`/`SetTransparencyFactor()` (**transparency isn't supported on S60 3rd Edition devices** — returns `KErrNotSupported`), `SetShadowDisabled()`, `SetFaded()`/`FadeBehind()`, `Position()`, `AbsPosition()`, `Size()`.

Redraws come in two flavours:
* **Server-initiated** — something uncovered your window (another app, a dialog).
* **Client-initiated** — you call `iWindow->Invalidate([rect])`; WSERV then sends you a redraw event.

**All drawing should happen in response to a redraw event**:

```cpp
void CDrawer::Start()
    {
    iWs.RedrawReady(&iStatus);
    SetActive();
    }

void CDrawer::RunL()
    {
    if (iStatus != KErrNone) return;
    TWsRedrawEvent redraw;
    iWs.GetRedraw(redraw);                  // redraw.Handle(), redraw.Rect()
    Draw(redraw.Rect());
    Start();
    }

void CDrawer::DoCancel() { iWs.RedrawReadyCancel(); }
```

## 22.5 Graphics contexts and drawing

```cpp
// once, e.g. in ConstructL — don't recreate per redraw
iScreen = new (ELeave) CWsScreenDevice(iWs);
User::LeaveIfError(iScreen->Construct());          // Construct(screenNumber) for multiple screens
User::LeaveIfError(iScreen->CreateContext(iGc));   // CWindowGc*

void CDrawer::Draw(const TRect& aRect)
    {
    iGc->Activate(*iWindow);
    iWindow->BeginRedraw(aRect);                    // tell WSERV which area we're redrawing
    iGc->SetBrushStyle(CGraphicsContext::ESolidBrush);
    iGc->SetBrushColor(KRgbWhite);
    iGc->Clear();
    iGc->SetPenColor(KRgbRed);
    iGc->SetPenSize(TSize(2, 2));
    iGc->DrawRect(TRect(5, 5, 60, 40));
    iGc->DrawEllipse(TRect(20, 20, 90, 90));
    iWindow->EndRedraw();
    iGc->Deactivate();
    iWs.Flush();                                    // usually automatic
    }
```

In a GUI control you don't do any of this: CONE calls your `CCoeControl::Draw(const TRect&) const` between `BeginRedraw`/`EndRedraw` with `SystemGc()` already activated. Trigger redraws with `DrawNow()` (synchronous) or `DrawDeferred()` (invalidate).

`CWindowGc` / `CGraphicsContext` API:

| Group | Methods |
|-------|---------|
| State | `SetOrigin()`, `SetClippingRect()`/`SetClippingRegion()`, `CancelClippingRect()`, `Reset()` |
| Pen | `SetPenColor(TRgb)`, `SetPenSize(TSize)`, `SetPenStyle(ESolidPen/ENullPen/EDottedPen/EDashedPen…)` |
| Brush | `SetBrushColor()`, `SetBrushStyle(ESolidBrush/ENullBrush/…)`, `SetBrushOrigin()`, `UseBrushPattern(bitmap)` |
| Lines | `MoveTo()`, `MoveBy()`, `Plot()`, `DrawLine(p1, p2)`, `DrawLineTo()`, `DrawLineBy()`, `DrawPolyLine(array)` |
| Shapes | `DrawRect()`, `DrawRoundRect(rect, cornerSize)`, `DrawEllipse()`, `DrawArc()`, `DrawPie()`, `DrawPolygon(array [, fillRule])` (filled with the brush) |
| Fill | `Clear()`, `Clear(rect)` |
| Modes | `SetDrawMode(EDrawModePEN, EDrawModeXOR, EDrawModeAND…)`, `SetFaded()`, `SetFadingParameters()`, `MapColors()`, `SetOpaque()` |
| Text | `UseFont()`, `DiscardFont()`, `DrawText()`, `DrawTextVertical()`, `DrawTextExtended()`, `SetUnderlineStyle()`, `SetStrikethroughStyle()`, `SetCharJustification()`, `SetWordJustification()` |
| Bitmaps | `BitBlt(point, bitmap [, srcRect])`, `BitBltMasked(point, bitmap, srcRect, mask, invertMask)`, `DrawBitmap(rect, bitmap [, srcRect])` (scaled), `DrawBitmapMasked()` |

Colours: `TRgb(r, g, b [, alpha])`, constants `KRgbBlack`, `KRgbWhite`, `KRgbRed`, `KRgbGreen`, `KRgbBlue`, `KRgbYellow`, `KRgbCyan`, `KRgbMagenta`, `KRgbGray`, `KRgbDarkGray`… (16 in `gdi.h`). In S60 UIs prefer **skin colours** (`AknsUtils::GetCachedColor()`).

WSERV buffers drawing commands client-side and flushes when needed; `RWsSession::Flush()` forces it, `SetAutoFlush(ETrue)` disables buffering (slow — only for debugging).

## 22.6 Fonts

* Font formats: **TrueType** (via the FreeType rasteriser / Open Font System plug-ins) and Symbian **GDR** bitmap fonts (`fnttran` converts BDF→GDR).
* The **Font and Bitmap Server** (FBSERV, `fbserv.exe`, SID `0x10003A16`) loads fonts once and shares them between processes. WSERV/`CWsScreenDevice` wrap the common FBSERV font calls.
* Sizes are specified in **twips** (1/20 point = 1/1440 inch = 1/567 cm), device-independent; pixels depend on the screen.

```cpp
// enumerate typefaces
TInt count = iScreen->NumTypefaces();
TTypefaceSupport support;
for (TInt i = 0; i < count; ++i)
    {
    iScreen->TypefaceSupport(support, i);      // iTypeface.iName, iIsScalable, iNumHeights, iMin/MaxHeightInTwips
    for (TInt h = 0; h < support.iNumHeights; ++h)
        TInt twips = iScreen->FontHeightInTwips(i, h);
    }

// get and use a font
_LIT(KTypeface, "Series 60 Sans");
TFontSpec spec(KTypeface, 220);                 // 220 twips
spec.iFontStyle.SetStrokeWeight(EStrokeWeightBold);
CFont* font = NULL;
User::LeaveIfError(iScreen->GetNearestFontInTwips(font, spec));   // also ...ToDesignHeightInPixels() etc.
iGc->UseFont(font);                             // drawing text without UseFont → panic
_LIT(KText, "Symbian");
iGc->DrawText(KText, TPoint(10, font->AscentInPixels() + 10));
iGc->DrawText(KText, TRect(0, 50, 100, 80), font->AscentInPixels(), CGraphicsContext::ECenter);  // clears the box first
iGc->DrawTextVertical(KText, TPoint(font->DescentInPixels(), 0), EFalse);
iGc->DiscardFont();
iScreen->ReleaseFont(font);                     // always release
```

`CFont` metrics: `HeightInPixels()`, `AscentInPixels()`, `DescentInPixels()`, `TextWidthInPixels(text)`, `TextCount(text, width)` (how many characters fit), `MaxCharWidthInPixels()`. Install custom fonts with `AddFile()` / `RemoveFile()`, or ship them in `\resource\fonts\`.

S60 UI code normally uses **layout fonts**: `AknLayoutUtils::FontFromId(EAknLogicalFontPrimaryFont)`, `CEikonEnv::Static()->NormalFont()`/`TitleFont()`/`DenseFont()` — they adapt to resolution and theme and don't need releasing.

## 22.7 Bitmaps

`CFbsBitmap` (`fbs.h`, `fbscli.lib`) — a handle to an image held by FBSERV (shared between clients, reference-counted; the session to FBSERV is managed automatically).

| Method | |
|--------|-|
| `Create(TSize, TDisplayMode)` | New blank bitmap |
| `Load(mbmFile, index [, shareIfLoaded])`, `LoadAndCompress()` | From an `.mbm` |
| `Duplicate(handle)` | Attach to an existing bitmap (e.g. passed from another process) |
| `Handle()` | For passing to other clients |
| `Compress()`, `CompressInBackground()`, `IsCompressedInRAM()`, `IsLargeBitmap()` | Memory |
| `SizeInPixels()`, `SizeInTwips()`, `SetSizeInTwips()`, `Resize()`, `DisplayMode()`, `SetDisplayMode()` (only to lower depth), `IsMonochrome()` | Geometry/format |
| `GetPixel()`, `GetScanLine()`, `SetScanLine()`, `GetVerticalScanLine()`, `DataAddress()` (with `LockHeap()`/`UnlockHeap()` or `TBitmapUtil`) | Pixel access |
| `Save()`, `StoreL()` | To `.mbm` |
| `Reset()` | Release |

For heavy pixel work use **`TBitmapUtil`** (`Begin(point)` … `GetPixel()`/`SetPixel()`/`IncXPos()` … `End()`) — FBSERV guarantees the data won't move meanwhile.

### Drawing *onto* a bitmap (off-screen buffers, double-buffering)

```cpp
CFbsBitmap* bmp = new (ELeave) CFbsBitmap;
CleanupStack::PushL(bmp);
User::LeaveIfError(bmp->Create(TSize(176, 208), EColor16MU));
CFbsBitmapDevice* dev = CFbsBitmapDevice::NewL(bmp);   // bitdev.h, bitgdi.lib
CleanupStack::PushL(dev);
CFbsBitGc* gc = NULL;
User::LeaveIfError(dev->CreateContext(gc));            // or CFbsBitGc::NewL() + Activate(dev)
CleanupStack::PushL(gc);
gc->SetPenColor(KRgbRed);
gc->SetPenSize(TSize(10, 10));
gc->DrawRoundRect(TRect(bmp->SizeInPixels()), TSize(50, 50));
CleanupStack::PopAndDestroy(2, dev);                   // gc, dev
// later, in a redraw:  iGc->BitBlt(TPoint(0, 0), bmp);  or DrawBitmap(rect, bmp) to scale
```

Double-buffering (draw a frame into an off-screen bitmap, then `BitBlt` it in `Draw()`) eliminates flicker in games and custom controls.

### Loading images
* `.mbm` (built by `bmconv`/`START BITMAP` in the MMP) → `CFbsBitmap::Load()`.
* `.mif` / SVG icons → `AknIconUtils::CreateIconL(bitmap, mask, file, id, maskId)` + **`AknIconUtils::SetSize()`** (required before drawing).
* JPEG/PNG/GIF/BMP… → **Image Conversion Library**: `CImageDecoder::FileNewL(fs, file)` then async `Convert(&status, *bitmap [, *mask])` (`imageconversion.lib`); encode with `CImageEncoder`. Scaling/rotation: `CBitmapScaler`, `CBitmapRotator` (`bitmaptransforms.lib`).
* Screenshots: `iScreen->CopyScreenToBitmap(bmp)`.

## 22.8 Advanced features (brief)

| Feature | API | Use |
|---------|-----|-----|
| Direct Screen Access (DSA) | `CDirectScreenAccess` + `MDirectScreenAccess` callbacks (`Restart`/`AbortNow`), or `RDirectScreenAccess` (async) | Games/video drawing directly to the frame buffer; WSERV tells you when another window overlaps so you must stop |
| Sprites | `RWsSprite`, `RWsPointerCursor` | Images that move over windows without causing redraws (cursors, simple animation) |
| Anim DLLs | `CAnimDll`, `CAnim`/`CWindowAnim`/`CSpriteAnim` (loaded into WSERV), client `RAnimDll`/`RAnim` | Smooth animation driven by WSERV's high-priority timers; raw event interception |
| OpenGL ES 1.x / EGL | `GLES/gl.h`, `EGL/egl.h` (`libgles_cm.lib`, `libegl.lib`) | 3D (and hardware-accelerated 2D) on devices with GPUs (N95, N8…) |
| OpenVG | `VG/openvg.h` | Vector graphics (Symbian^3) |
| Screen rotation | `EEventScreenDeviceChanged`; `CWsScreenDevice::SetScreenMode()`; in Avkon `HandleResourceChangeL(KEikDynamicLayoutVariantSwitch)`, `CAknAppUi::SetOrientationL()` | Landscape/portrait |

## See also
* [Applications (GUI architecture)](12-applications.md#123-s60-gui-application-architecture)
* [Active objects](11-active-objects.md)

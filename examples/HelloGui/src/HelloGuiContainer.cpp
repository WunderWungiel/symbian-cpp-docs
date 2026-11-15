#include <eikenv.h>
#include <gdi.h>
#include "HelloGuiContainer.h"

CHelloGuiContainer* CHelloGuiContainer::NewL(const TRect& aRect)
    {
    CHelloGuiContainer* self = new (ELeave) CHelloGuiContainer;
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    CleanupStack::Pop(self);
    return self;
    }

CHelloGuiContainer::CHelloGuiContainer()
    {
    }

void CHelloGuiContainer::ConstructL(const TRect& aRect)
    {
    _LIT(KInitial, "Options > Say hello");
    iText = KInitial().AllocL();
    CreateWindowL();
    SetRect(aRect);
    ActivateL();
    }

CHelloGuiContainer::~CHelloGuiContainer()
    {
    delete iText;
    }

void CHelloGuiContainer::SetTextL(const TDesC& aText)
    {
    HBufC* text = aText.AllocL();   // allocate first: if this leaves, iText is untouched
    delete iText;
    iText = text;
    DrawDeferred();
    }

void CHelloGuiContainer::Draw(const TRect& /*aRect*/) const
    {
    CWindowGc& gc = SystemGc();
    gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
    gc.SetBrushColor(KRgbWhite);
    gc.Clear(Rect());

    const CFont* font = CEikonEnv::Static()->NormalFont();
    gc.UseFont(font);
    gc.SetPenColor(KRgbBlack);
    const TInt baseline = Rect().Height() / 2;
    gc.DrawText(*iText, Rect(), baseline, CGraphicsContext::ECenter);
    gc.DiscardFont();
    }

TKeyResponse CHelloGuiContainer::OfferKeyEventL(const TKeyEvent& /*aKeyEvent*/, TEventCode /*aType*/)
    {
    return EKeyWasNotConsumed;   // let the framework handle softkeys etc.
    }

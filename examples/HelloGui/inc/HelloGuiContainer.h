#ifndef __HELLOGUICONTAINER_H__
#define __HELLOGUICONTAINER_H__

#include <coecntrl.h>

class CHelloGuiContainer : public CCoeControl
    {
public:
    static CHelloGuiContainer* NewL(const TRect& aRect);
    ~CHelloGuiContainer();
    void SetTextL(const TDesC& aText);
private: // from CCoeControl
    void Draw(const TRect& aRect) const;
    TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
private:
    CHelloGuiContainer();
    void ConstructL(const TRect& aRect);
private:
    HBufC* iText;   // owned
    };

#endif

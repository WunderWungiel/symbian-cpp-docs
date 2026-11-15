#ifndef __HELLOGUIAPPUI_H__
#define __HELLOGUIAPPUI_H__

#include <aknappui.h>

class CHelloGuiContainer;

class CHelloGuiAppUi : public CAknAppUi
    {
public:
    void ConstructL();
    ~CHelloGuiAppUi();
private: // from CEikAppUi
    void HandleCommandL(TInt aCommand);
    void HandleResourceChangeL(TInt aType);
private:
    CHelloGuiContainer* iContainer;   // owned
    };

#endif

#include <avkon.hrh>
#include <aknnotewrappers.h>   // CAknInformationNote
#include "HelloGuiAppUi.h"
#include "HelloGuiContainer.h"
#include "HelloGui.hrh"

void CHelloGuiAppUi::ConstructL()
    {
    BaseConstructL(EAknEnableSkin);
    iContainer = CHelloGuiContainer::NewL(ClientRect());
    AddToStackL(iContainer);   // receive key events
    }

CHelloGuiAppUi::~CHelloGuiAppUi()
    {
    if (iContainer)
        {
        RemoveFromStack(iContainer);
        delete iContainer;
        }
    }

void CHelloGuiAppUi::HandleCommandL(TInt aCommand)
    {
    switch (aCommand)
        {
        case EEikCmdExit:
        case EAknSoftkeyExit:
            Exit();
            break;
        case EHelloGuiCmdHello:
            {
            _LIT(KHello, "Hello from Symbian C++!");
            iContainer->SetTextL(KHello);
            CAknInformationNote* note = new (ELeave) CAknInformationNote;
            note->ExecuteLD(KHello);   // "D": the note deletes itself
            }
            break;
        default:
            break;
        }
    }

void CHelloGuiAppUi::HandleResourceChangeL(TInt aType)
    {
    CAknAppUi::HandleResourceChangeL(aType);
    if (aType == KEikDynamicLayoutVariantSwitch && iContainer)   // rotation / resolution change
        {
        iContainer->SetRect(ClientRect());
        }
    }

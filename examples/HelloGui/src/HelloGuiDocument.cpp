#include "HelloGuiDocument.h"
#include "HelloGuiAppUi.h"

CHelloGuiDocument* CHelloGuiDocument::NewL(CEikApplication& aApp)
    {
    return new (ELeave) CHelloGuiDocument(aApp);   // nothing that can leave to construct
    }

CHelloGuiDocument::CHelloGuiDocument(CEikApplication& aApp) : CAknDocument(aApp)
    {
    }

CEikAppUi* CHelloGuiDocument::CreateAppUiL()
    {
    return new (ELeave) CHelloGuiAppUi;   // the framework calls ConstructL()
    }

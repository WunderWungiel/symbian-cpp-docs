#include "HelloGuiApplication.h"
#include "HelloGuiDocument.h"

TUid CHelloGuiApplication::AppDllUid() const
    {
    return KUidHelloGuiApp;
    }

CApaDocument* CHelloGuiApplication::CreateDocumentL()
    {
    return CHelloGuiDocument::NewL(*this);
    }

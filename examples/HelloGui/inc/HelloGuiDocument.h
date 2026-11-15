#ifndef __HELLOGUIDOCUMENT_H__
#define __HELLOGUIDOCUMENT_H__

#include <akndoc.h>

class CHelloGuiDocument : public CAknDocument
    {
public:
    static CHelloGuiDocument* NewL(CEikApplication& aApp);
    CEikAppUi* CreateAppUiL();
private:
    CHelloGuiDocument(CEikApplication& aApp);
    };

#endif

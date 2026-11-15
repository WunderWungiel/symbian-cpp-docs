#ifndef __HELLOGUIAPPLICATION_H__
#define __HELLOGUIAPPLICATION_H__

#include <aknapp.h>

const TUid KUidHelloGuiApp = { 0xE6205EE3 };   // must equal UID3 in the MMP

class CHelloGuiApplication : public CAknApplication
    {
public:
    TUid AppDllUid() const;
protected:
    CApaDocument* CreateDocumentL();
    };

#endif

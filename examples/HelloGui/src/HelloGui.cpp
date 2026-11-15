#include <eikstart.h>
#include "HelloGuiApplication.h"

LOCAL_C CApaApplication* NewApplication()
    {
    return new CHelloGuiApplication;   // the framework handles NULL
    }

GLDEF_C TInt E32Main()
    {
    return EikStart::RunApplication(NewApplication);
    }

#include <e32base.h>
#include <e32std.h>
#include <e32cons.h>
#include "Ticker.h"

_LIT(KTextConsoleTitle, "HelloConsole");
_LIT(KTextFailed, "Failed, leave code = %d\n");
_LIT(KTextPressAnyKey, "[press any key]\n");
_LIT(KTextHello, "Hello, Symbian!\n");

LOCAL_C void MainL(CConsoleBase& aConsole)
    {
    aConsole.Write(KTextHello);

    CActiveScheduler* scheduler = new (ELeave) CActiveScheduler;
    CleanupStack::PushL(scheduler);
    CActiveScheduler::Install(scheduler);

    CTicker* ticker = CTicker::NewLC(aConsole, 3);
    ticker->Start();                    // a request must be outstanding...
    CActiveScheduler::Start();          // ...before starting the loop

    CleanupStack::PopAndDestroy(2, scheduler);   // ticker, scheduler
    }

GLDEF_C TInt E32Main()
    {
    __UHEAP_MARK;
    CTrapCleanup* cleanup = CTrapCleanup::New();
    if (!cleanup)
        return KErrNoMemory;

    CConsoleBase* console = NULL;
    TRAPD(err, console = Console::NewL(KTextConsoleTitle,
                                       TSize(KConsFullScreen, KConsFullScreen)));
    if (err == KErrNone)
        {
        TRAP(err, MainL(*console));
        if (err != KErrNone)
            console->Printf(KTextFailed, err);
        console->Write(KTextPressAnyKey);
        console->Getch();
        delete console;
        }

    delete cleanup;
    __UHEAP_MARKEND;
    return err;
    }

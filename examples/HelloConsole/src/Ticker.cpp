#include "Ticker.h"
#include <e32cons.h>

const TInt KTickInterval = 1000000;   // 1 s

CTicker* CTicker::NewLC(CConsoleBase& aConsole, TInt aTicks)
    {
    CTicker* self = new (ELeave) CTicker(aConsole, aTicks);
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
    }

CTicker::CTicker(CConsoleBase& aConsole, TInt aTicks)
    : CActive(EPriorityStandard), iConsole(aConsole), iRemaining(aTicks)
    {
    }

void CTicker::ConstructL()
    {
    User::LeaveIfError(iTimer.CreateLocal());
    CActiveScheduler::Add(this);
    }

CTicker::~CTicker()
    {
    Cancel();          // always first
    iTimer.Close();
    }

void CTicker::Start()
    {
    Cancel();
    IssueRequest();
    }

void CTicker::IssueRequest()
    {
    iTimer.After(iStatus, KTickInterval);
    SetActive();
    }

void CTicker::RunL()
    {
    User::LeaveIfError(iStatus.Int());
    _LIT(KTick, "Tick %d\n");
    iConsole.Printf(KTick, iRemaining);
    if (--iRemaining > 0)
        {
        IssueRequest();
        }
    else
        {
        CActiveScheduler::Stop();   // makes CActiveScheduler::Start() return
        }
    }

void CTicker::DoCancel()
    {
    iTimer.Cancel();
    }

TInt CTicker::RunError(TInt aError)
    {
    _LIT(KError, "Timer error %d\n");
    iConsole.Printf(KError, aError);
    CActiveScheduler::Stop();
    return KErrNone;   // handled
    }

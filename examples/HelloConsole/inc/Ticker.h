#ifndef __TICKER_H__
#define __TICKER_H__

#include <e32base.h>   // CActive
#include <e32std.h>    // RTimer

class CConsoleBase;

/**
 * Active object that ticks once per second a given number of times,
 * then stops the active scheduler.
 */
class CTicker : public CActive
    {
public:
    static CTicker* NewLC(CConsoleBase& aConsole, TInt aTicks);
    ~CTicker();
    void Start();

private:
    CTicker(CConsoleBase& aConsole, TInt aTicks);
    void ConstructL();
    void IssueRequest();

private: // from CActive
    void RunL();
    void DoCancel();
    TInt RunError(TInt aError);

private:
    CConsoleBase& iConsole;   // not owned
    TInt          iRemaining;
    RTimer        iTimer;     // owned handle
    };

#endif // __TICKER_H__

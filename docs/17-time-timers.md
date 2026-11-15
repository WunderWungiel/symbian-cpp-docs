# 17. Time, Dates and Timers

> **Scope:** `TTime`, `TDateTime`, intervals, parsing and formatting, locale; `RTimer`, `CTimer`, `CPeriodic`, `CHeartbeat`, `CIdle`, `CDeltaTimer`, synchronous delays.

## 17.1 `TTime`

A T-class wrapping a `TInt64` = **microseconds since 00:00, 1 January year 0 AD** (negative = BC).

```cpp
TTime now;
now.HomeTime();             // local time (time zone + DST), as shown on the clock
TTime utc;
utc.UniversalTime();        // UTC
TTimeIntervalSeconds offset = User::UTCOffset();   // local - UTC; don't compute by subtraction (clock moves)

TTime t(TInt64(3600000000));   // from microseconds
TInt64 us = now.Int64();
```

Queries: `DayNoInWeek()` (Monday = 0, `TDay` enum), `DayNoInMonth()` (0-based), `DaysInMonth()`, `DayNoInYear([start])` (1-based), `WeekNoInYear([start] [, TFirstWeekRule])` (default: week containing ≥ 4 days of the new year).

Arithmetic and comparison:

```cpp
now += TTimeIntervalYears(3);
now -= TTimeIntervalHours(3);
TTime later = now + TTimeIntervalMinutes(10);
if (later > now) { ... }
now.RoundUpToNextMinute();

TTimeIntervalMicroSeconds diff = later.MicroSecondsFrom(now);
TTimeIntervalSeconds s;
User::LeaveIfError(later.SecondsFrom(now, s));      // KErrOverflow if too large
TTimeIntervalMinutes m;  later.MinutesFrom(now, m);
TTimeIntervalDays d = later.DaysFrom(now);
```

Interval classes: `TTimeIntervalYears`, `…Months`, `…Days`, `…Hours`, `…Minutes`, `…Seconds`, `TTimeIntervalMicroSeconds32` (32-bit, max ~35 minutes — **used by most timer APIs**), `TTimeIntervalMicroSeconds` (64-bit). Note: comparing two intervals of **different types** compares only the raw numbers (`TTimeIntervalYears(3) == TTimeIntervalHours(3)` is true).

## 17.2 `TDateTime`

Seven components: year, month (`TMonth`, **0-based**: `EJanuary` = 0), day (**0-based**: the 1st is 0), hour, minute, second, microsecond.

```cpp
TTime time;
time.HomeTime();
TDateTime dt = time.DateTime();
TInt year = dt.Year();
TMonth month = dt.Month();
TInt dayOfMonth = dt.Day() + 1;     // human-readable day
dt.SetYear(1984);
time = dt;                          // TTime from TDateTime

TDateTime birthday(1984, ENovember, 25 /*= 26th*/, 20, 0, 0, 0);
User::LeaveIfError(dt.Set(2010, EFebruary, 27, 12, 0, 0, 0));   // returns KErrGeneral if invalid
TInt err = dt.SetYearLeapCheck(2011);   // fails if the date is 29 Feb and the year isn't leap
```

## 17.3 Parsing and formatting

```cpp
TTime t;
TInt res = t.Parse(_L("26/11/1984 20:00"));      // order depends on locale date format
if (res >= 0 && (res & EParseDatePresent) && (res & EParseTimePresent))
    {
    TBuf<32> buf;
    _LIT(KFormat, "%F%D/%M/%Y %H:%T");           // %F = use the fixed (non-locale) order given
    t.FormatL(buf, KFormat);                     // "26/11/1984 20:00"
    }

// Locale-aware formats
TBuf<40> dateStr;
t.FormatL(dateStr, _L("%/0%1%/1%2%/2%3%/3"));    // date in the user's format
TBuf<40> timeStr;
t.FormatL(timeStr, _L("%-B%:0%J%:1%T%:2%S%:3%+B"));   // time in the user's format (12/24h)
```

Common `FormatL` directives: `%D` day, `%M` month number, `%N` month name, `%Y` 4-digit year, `%y` 2-digit, `%H` 24-h hour, `%I` 12-h hour, `%T` minute, `%S` second, `%C` microseconds, `%E` day name, `%A`/`%B` am/pm, `%F` fixed order, `%*` abbreviate, `%/n` and `%:n` locale separators, `%1`…`%3` date components in locale order. There are also predefined format strings in the AVKON resources (`R_QTN_DATE_USUAL`, `R_QTN_TIME_USUAL`), loaded with `StringLoader`.

`TLocale` exposes the user's settings: `DateFormat()`, `TimeFormat()`, `DateSeparator(i)`, `TimeSeparator(i)`, `DecimalSeparator()`, `ThousandsSeparator()`, `CurrencySymbol` via `TCurrencySymbol`, `StartOfWeek()`, `UniversalTimeOffset()`. `TDayName`, `TMonthName` give localized names.

Changing the system time requires `WriteDeviceData` (`User::SetHomeTime()`, `User::SetUTCTime()`).

## 17.4 `RTimer` — the asynchronous timer service

`RTimer` is the basic async timer (an R-class handle to a kernel timer), normally a member of an active object.

| Method | Completes when… |
|--------|-----------------|
| `CreateLocal()` | (initialise; required) |
| `After(status, TTimeIntervalMicroSeconds32)` | an interval has elapsed (≈ 1/64 s resolution on many devices; doesn't count while the phone is off) |
| `AfterTicks(status, ticks)` | a number of system ticks have elapsed |
| `HighRes(status, interval)` | interval elapsed with high resolution (~1 ms or better) |
| `At(status, TTime local)` / `AtUTC(status, TTime utc)` | the given time is reached. **Completes with `KErrAbort` if the system time is changed**, `KErrUnderflow` if the time is already past, `KErrOverflow` if too far ahead. Can wake the device. |
| `Lock(status, TTimerLockSpec)` | the next given twelfth of a second (`EOneOClock`…`ETwelveOClock`) |
| `Inactivity(status, TTimeIntervalSeconds)` | the user has been inactive for that long (keys/pointer). If already inactive longer, waits for the next inactivity period. `User::InactivityTime()` / `User::ResetInactivityTime()` (keeps the backlight on). |
| `Cancel()` | — (completes outstanding request with `KErrCancel`) |
| `Close()` | — |

Only one request at a time per `RTimer` (`KERN-EXEC 15` otherwise).

### Detecting system time changes

```cpp
class MTimeObserver
    {
public:
    virtual void SystemTimeChanged() = 0;
    virtual void TimerStopped(TInt aError) = 0;
    };

class CTimeChangeWatcher : public CActive
    {
public:
    static CTimeChangeWatcher* NewL(MTimeObserver& aObserver);
    ~CTimeChangeWatcher() { Cancel(); iTimer.Close(); }
    void Start()
        {
        Cancel();
        TTime now;
        now.HomeTime();
        iTimer.At(iStatus, now + TTimeIntervalHours(1));
        SetActive();
        }
private:
    CTimeChangeWatcher(MTimeObserver& aObserver) : CActive(EPriorityStandard), iObserver(aObserver) {}
    void ConstructL() { User::LeaveIfError(iTimer.CreateLocal()); CActiveScheduler::Add(this); }
    void RunL()
        {
        switch (iStatus.Int())
            {
            case KErrAbort: iObserver.SystemTimeChanged(); Start(); break;
            case KErrNone:  Start(); break;                       // an hour passed; re-arm
            default:        iObserver.TimerStopped(iStatus.Int()); break;
            }
        }
    void DoCancel() { iTimer.Cancel(); }
    RTimer iTimer;
    MTimeObserver& iObserver;
    };
```

(An alternative on 9.x is subscribing to the system time-change property, or `CEnvironmentChangeNotifier` with `EChangesSystemTime`/`EChangesLocale`/`EChangesMidnightCrossover`.)

Remember `RunL()` can run later than the timer fired if other AOs are busy.

## 17.5 `CTimer`

A partially implemented `CActive` that owns an `RTimer`: provides `ConstructL()` (creates the timer — **call it from your `ConstructL()`**), `DoCancel()`, the destructor, and `After()`, `At()`, `AtUTC()`, `HighRes()`, `Lock()`, `Inactivity()` using its own `iStatus` (and calling `SetActive()` for you). You derive and implement only `RunL()`.

```cpp
class CMyTimeout : public CTimer
    {
public:
    static CMyTimeout* NewL(MTimeoutObserver& aObs)
        {
        CMyTimeout* self = new (ELeave) CMyTimeout(aObs);
        CleanupStack::PushL(self);
        self->ConstructL();                  // CTimer::ConstructL()
        CleanupStack::Pop(self);
        return self;
        }
private:
    CMyTimeout(MTimeoutObserver& aObs) : CTimer(EPriorityStandard), iObs(aObs)
        { CActiveScheduler::Add(this); }
    void RunL() { iObs.TimedOut(); }
    MTimeoutObserver& iObs;
    };

iTimeout->After(5000000);   // 5 s; call Cancel() before re-arming if it may be active
```

## 17.6 `CPeriodic` — periodic callback

```cpp
class CTicker : public CBase
    {
public:
    void ConstructL() { iPeriodic = CPeriodic::NewL(CActive::EPriorityStandard); }
    ~CTicker()
        {
        if (iPeriodic) iPeriodic->Cancel();
        delete iPeriodic;
        }
    void Start()
        {
        const TTimeIntervalMicroSeconds32 KDelay(0);           // first tick after…
        const TTimeIntervalMicroSeconds32 KPeriod(60000000);   // …then every 60 s
        iPeriodic->Start(KDelay, KPeriod, TCallBack(CTicker::Tick, this));
        }
    void Stop() { iPeriodic->Cancel(); }
private:
    static TInt Tick(TAny* aSelf)
        {
        static_cast<CTicker*>(aSelf)->DoTick();
        return ETrue;            // return value is ignored by CPeriodic (it keeps going until Cancel)
        }
    void DoTick();
    CPeriodic* iPeriodic;
    };
```

* The callback must be a static member or free function `TInt F(TAny*)`; pass `this` as the `TCallBack` argument.
* `CPeriodic` keeps firing until `Cancel()`; don't call `Start()` while active. Some books (including the source book) say returning `EFalse` from the callback stops it — in the actual implementation `CPeriodic` ignores the return value, so **call `Cancel()`** (it's safe to do so from inside the callback). **The callback cannot leave** — trap internally.
* Periods aren't guaranteed exact; missed ticks aren't reported. For animations/games use `CPeriodic` with ~20–50 ms, or `CHeartbeat` to detect lag.

## 17.7 `CHeartbeat` — once-a-second, synchronised

Uses `RTimer::Lock()`. Calls `MBeating::Beat()` every second at a chosen twelfth of a second, and `MBeating::Synchronize()` when beats were missed (e.g. the scheduler was busy):

```cpp
class CClockView : public CBase, public MBeating
    {
public:
    void ConstructL() { iHeartbeat = CHeartbeat::NewL(CActive::EPriorityStandard); }
    ~CClockView() { if (iHeartbeat) iHeartbeat->Cancel(); delete iHeartbeat; }
    void Start() { iHeartbeat->Start(ETwelveOClock, this); }    // on the second
private:
    void Beat()        { /* update the clock display */ }
    void Synchronize() { /* beats were missed — re-read the time */ }
    CHeartbeat* iHeartbeat;
    };
```

## 17.8 Other helpers

* **`CIdle`** — calls a `TCallBack` repeatedly while no higher-priority AO is ready; return `ETrue` to be called again, `EFalse` to stop. Ideal for background processing in the UI thread.
* **`CAsyncCallBack`** — one-shot deferred callback (`CallBack()`), e.g. to delete an object after the current call stack unwinds.
* **`CDeltaTimer`** — one timer serving a queue of `TDeltaTimerEntry` callbacks with different delays.
* **Synchronous:** `User::After(TTimeIntervalMicroSeconds32)`, `User::AfterHighRes()`, `User::At(TTime)` block the **whole thread** — never in a UI thread. Don't turn `RTimer::After()` into a sync call with `User::WaitForRequest()`; use `User::After()`.
* **Measuring time:** `User::TickCount()` (ticks, `HAL::Get(HAL::ESystemTickPeriod, period)` for the tick length in µs), `User::NTickCount()` (nanokernel ticks, ms on many devices), `User::FastCounter()` + `HAL::EFastCounterFrequency` for high-resolution profiling.
* **Alarms** that survive app exit / wake the phone: the Alarm Server (`RASCliSession`, `TASShdAlarm`) — requires appropriate capabilities.

## See also
* [Active objects](11-active-objects.md)

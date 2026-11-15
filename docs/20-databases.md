# 20. Databases: Symbian DBMS (and Symbian SQL)

> **Scope:** the classic Symbian **DBMS** API (`d32dbms.h`, `edbms.lib`) — creating databases, tables, columns, indexes, cursors, reading/writing rows, BLOBs, SQL views, shared access via `RDbs`, transactions, incremental operations. A short note on **Symbian SQL** (SQLite).

## 20.1 Choosing a database engine

| Engine | Availability | Notes |
|--------|-------------|-------|
| **DBMS** (`edbms`) | All Symbian versions | Relational, single-file, object API + a **limited SQL subset**. Embedded or client–server (`edbsrv.exe`, UID3 `0x100012A5`, transient server). |
| **Symbian SQL** (SQLite, `RSqlDatabase`, `sqldb.lib`) | Built into S60 3rd FP2 (9.3)+; installable SIS for FP1 (9.2); not on 9.1 | Much fuller SQL. Recommended for new code on 9.3+. |
| Open C `libsqlite3` / your own SQLite build | Via Open C/P.I.P.S. | For ported code |
| Oracle Database Lite | Commercial | Sync with Oracle servers |

The rest of this chapter covers DBMS.

## 20.2 Access modes

* **Exclusive read/write** or **shared read-only** → use DBMS **directly** (embedded; faster, no IPC).
* **Shared with at least one writer** → *all* clients must go through the **DBMS server** (`RDbs` session).

## 20.3 Database classes

* `RDbDatabase` — abstract base: schema operations, transactions, SQL `Execute()`.
* `RDbStoreDatabase` — a database inside a stream of a `CPermanentFileStore` (direct access only; supports `CompressL()`/`DecompressL()` for read-only compressed DBs — not for encrypted DBs; indexes aren't compressed).
* `RDbNamedDatabase` — a database by file name (creates the store itself; format "epoc"); direct or via `RDbs`. The usual choice.

```cpp
// RDbNamedDatabase, direct access
_LIT(KDbName, "c:\\private\\E1234567\\mydb.db");
_LIT(KDbFormat, "[101010FF]");            // "[UID]" — stored as the file's UID3
RDbNamedDatabase db;
TInt err = db.Open(fs, KDbName);
if (err == KErrNotFound)
    err = db.Create(fs, KDbName, KDbFormat);   // or Replace()
User::LeaveIfError(err);
CleanupClosePushL(db);
// ...
CleanupStack::PopAndDestroy(&db);

// RDbStoreDatabase
CPermanentFileStore* store = CPermanentFileStore::ReplaceLC(fs, KDbName, EFileWrite);
store->SetTypeL(TUidType(store->Layout()));
RDbStoreDatabase sdb;
TStreamId id = sdb.CreateL(store);          // or OpenL(store, id)
CleanupClosePushL(sdb);
store->SetRootL(id);
store->CommitL();
```

`TUid::Name()` produces the `"[xxxxxxxx]"` format string: `TUidName n = TUid::Uid(0x101010FF).Name();`

## 20.4 Tables and columns

Naming rules (tables, columns, indexes): case-insensitive, start with a letter, letters/digits/underscore only, **≤ 64 characters**. Max **record size 8200 bytes** (limits column count, e.g. 32 × `Text8[255]`). Number of tables/rows limited only by resources.

| `TDbColType` | SQL type | Range / size | Auto-increment | Indexable |
|--------------|----------|--------------|----------------|-----------|
| `EDbColBit` | `BIT` | 0/1 | yes | yes |
| `EDbColInt8` / `EDbColUint8` | `TINYINT` / `UNSIGNED TINYINT` | 1 byte | yes | yes |
| `EDbColInt16` / `EDbColUint16` | `SMALLINT` / `UNSIGNED SMALLINT` | 2 bytes | yes | yes |
| `EDbColInt32` / `EDbColUint32` | `INTEGER` / `UNSIGNED INTEGER` (`COUNTER` = Uint32 + auto-increment) | 4 bytes | yes | yes |
| `EDbColInt64` | `BIGINT` | 8 bytes | no | yes |
| `EDbColReal32` | `REAL` | 4 bytes | no | yes |
| `EDbColReal64` | `FLOAT`, `DOUBLE [PRECISION]` | 8 bytes | no | yes |
| `EDbColDateTime` | `DATE`, `TIME`, `TIMESTAMP` | 8 bytes | no | yes |
| `EDbColText8` / `EDbColText16` (`EDbColText` = 16-bit) | `CHAR(n)`, `VARCHAR(n)` | ≤ 255 chars | no | yes |
| `EDbColBinary` | `BINARY(n)`, `VARBINARY(n)` | ≤ 255 bytes | no | no |
| `EDbColLongText8/16` (`EDbColLongText`) | `LONG VARCHAR` | up to 2 GB | no | limited (prefix) |
| `EDbColLongBinary` | `LONG VARBINARY` | up to 2 GB | no | no |
| — | `DECIMAL`, `NUMERIC` | not supported | | |

```cpp
TDbCol colId(_L("IdxCol"), EDbColUint32);
colId.iAttributes = TDbCol::ENotNull | TDbCol::EAutoIncrement;
TDbCol colName(_L("StrCol"), EDbColText16, 20);
colName.iAttributes = 0;                 // ALWAYS initialise iAttributes (garbage → KErrNotSupported)
TDbCol colData(_L("DataCol"), EDbColLongBinary);
colData.iAttributes = 0;

CDbColSet* cols = CDbColSet::NewLC();
cols->AddL(colId);
cols->AddL(colName);
cols->AddL(colData);
_LIT(KTable, "Test");
User::LeaveIfError(db.CreateTable(KTable, *cols));      // overload with a CDbKey = primary key
// change schema: add a column and AlterTable
cols->AddL(TDbCol(_L("NewCol"), EDbColBit));
User::LeaveIfError(db.AlterTable(KTable, *cols));
CleanupStack::PopAndDestroy(cols);
```

* `CDbColSet`: `AddL()`, `Remove(name)`, `Clear()`, `Count()`, `Col(name)` → `const TDbCol*` or NULL, `ColNo(name)` → index or `KDbNullColNo` (**column numbers start at 1**), `operator[]`.
* `db.ColSetL(table)` returns the columns of an existing table (you delete it). `db.TableNamesL()` → `CDbTableNames*`; `db.DropTable(name)`.

## 20.5 Indexes

```cpp
CDbKey* key = CDbKey::NewLC();
key->AddL(TDbKeyCol(_L("IdxCol")));                       // ascending
key->AddL(TDbKeyCol(_L("StrCol"), 10, TDbKeyCol::EDesc)); // first 10 chars, descending
key->MakeUnique();                                        // MakePrimary() for a primary key
key->SetComparison(EDbCompareNormal);                     // or EDbCompareFolded / EDbCompareCollated
_LIT(KIndex, "TestIdx");
User::LeaveIfError(db.CreateIndex(KIndex, KTable, *key));
CleanupStack::PopAndDestroy(key);
```

`db.KeyL(index, table)`, `db.IndexNamesL(table)`, `db.DropIndex(index, table)`.

## 20.6 Row sets: `RDbTable`

`RDbRowSet` is the base for `RDbTable` (a table) and `RDbView` (an SQL query result).

```cpp
RDbTable tbl;
User::LeaveIfError(tbl.Open(db, KTable));                 // access: EUpdatable (default), EReadOnly, EInsertOnly
CleanupClosePushL(tbl);
User::LeaveIfError(tbl.SetIndex(KIndex));                 // order rows by an index (SetNoIndex() to undo)
```

### Navigation
A row set has positions **beginning** and **end** (not rows) plus a cursor.

| Method | |
|--------|-|
| `BeginningL()`, `EndL()` | Move to beginning/end |
| `FirstL()`, `LastL()`, `NextL()`, `PreviousL()` | Move; return `EFalse` when running off the set |
| `AtBeginning()`, `AtEnd()`, `AtRow()` | Where am I? |
| `CountL()`, `IsEmptyL()` | Size (`IsEmptyL` is faster) |
| `Bookmark()` → `TDbBookmark`, `GotoL(bookmark)` | Remember / return to a row |
| `SeekL(TDbSeekKey, RDbTable::TComparison)` | Search the **active index** (`ELessThan`, `ELessEqual`, `EEqualTo` default, `EGreaterEqual`, `EGreaterThan`); `TDbSeekMultiKey<N>` for composite keys |

```cpp
for (tbl.FirstL(); tbl.AtRow(); tbl.NextL()) { /* ... */ }

TDbSeekKey k(10);
if (tbl.SeekL(k)) { /* cursor on the row with IdxCol == 10 */ }
TDbSeekMultiKey<2> mk;
mk.Add(10);
mk.Add(_L("String"));
if (tbl.SeekL(mk, RDbTable::EGreaterEqual)) { /* ... */ }
```

### Reading
Call **`GetL()`** to load the current row, then the typed accessors by column number:

```cpp
CDbColSet* cs = tbl.ColSetL();
const TDbColNo noId   = cs->ColNo(_L("IdxCol"));
const TDbColNo noStr  = cs->ColNo(_L("StrCol"));
const TDbColNo noData = cs->ColNo(_L("DataCol"));
delete cs;

if (tbl.LastL())
    {
    tbl.GetL();
    TUint32 id = tbl.ColUint32(noId);
    TPtrC  str = tbl.ColDes(noStr);
    if (!tbl.IsColNull(noData))
        {
        RDbColReadStream in;                 // BLOB / long columns are read as streams
        in.OpenLC(tbl, noData);
        TMyStreamData sample;
        in >> sample;
        CleanupStack::PopAndDestroy(&in);
        }
    }
```

Accessors: `ColInt()`, `ColInt8/16/32/64()`, `ColUint()`, `ColUint8/16/32()`, `ColReal()`, `ColReal32/64()`, `ColTime()`, `ColDes()`, `ColDes8/16()`; info: `ColCount()`, `ColType(n)`, `ColDef(n)`, `ColSize(n)` (bytes), `ColLength(n)` (chars/bytes; 1 for numbers), `IsColNull(n)`.

### Writing

```cpp
tbl.InsertL();                                  // new row, columns NULL (auto-increment set automatically)
tbl.SetColL(noStr, _L("New String Value"));
tbl.PutL();                                     // validate + write + update indexes; cursor moves to the new row

tbl.InsertCopyL();                              // new row pre-filled from the current one
RDbColWriteStream out;
out.OpenLC(tbl, noData);
out << sample;
out.CommitL();
CleanupStack::PopAndDestroy(&out);
tbl.PutL();

if (tbl.FirstL())
    {
    tbl.UpdateL();                              // edit current row
    tbl.SetColNullL(noData);
    tbl.PutL();                                 // or tbl.Cancel()
    }

if (tbl.LastL())
    tbl.DeleteL();                              // cursor stays on a "ghost" until moved
```

## 20.7 SQL

DBMS SQL is a **subset**: single-table `SELECT … FROM t WHERE … ORDER BY …`, `INSERT`, `UPDATE`, `DELETE`, `CREATE/DROP TABLE`, `ALTER TABLE`, `CREATE [UNIQUE] INDEX`, `DROP INDEX`. **No** joins, unions, `DISTINCT`, aggregates (beyond what the SDK lists), triggers or procedures. See *SDK Help → DBMS → SQL*.

DDL and data-modifying DML via `Execute()` (returns an error, or the number of rows affected for DML):

```cpp
_LIT(KCreateTable, "CREATE TABLE Test (IdxCol COUNTER NOT NULL, StrCol VARCHAR(20), "
                   "DataCol LONG VARBINARY, NewCol BIT)");
User::LeaveIfError(db.Execute(KCreateTable));
_LIT(KCreateIndex, "CREATE UNIQUE INDEX TestIdx ON Test (IdxCol ASC, StrCol DESC)");
User::LeaveIfError(db.Execute(KCreateIndex));
TInt rows = db.Execute(_L("UPDATE Test SET NewCol = 1 WHERE IdxCol > 10"));
```

`UPDATE`/`DELETE` through SQL can be faster than C++ loops; **`INSERT` via SQL is much slower** than `InsertL()`/`PutL()`.

### Views (`SELECT`) — `RDbView`

```cpp
RDbView view;
_LIT(KQuery, "SELECT * FROM Test WHERE StrCol LIKE 'New*' ORDER BY IdxCol");
User::LeaveIfError(view.Prepare(db, TDbQuery(KQuery, EDbCompareFolded)));   // no pre-evaluation
CleanupClosePushL(view);
User::LeaveIfError(view.EvaluateAll());   // optional here; required for pre-evaluated views
while (view.NextL())
    {
    view.GetL();
    // view.ColDes(2) ...
    }
CleanupStack::PopAndDestroy(&view);
```

Evaluation modes (chosen by the `TDbWindow` passed to `Prepare()`):

| Mode | `Prepare(db, query, …)` | Behaviour |
|------|-------------------------|-----------|
| No pre-evaluation | no window | Rows are found on the fly as you move; all visited rows are kept |
| Full pre-evaluation | `KDbUnlimitedWindow` / `TDbWindow(TDbWindow::EUnlimited)` | You call `Evaluate()` (one engine-chosen block, e.g. ~257 rows) until `Unevaluated()` is false, or `EvaluateAll()`; all rows kept |
| Limited pre-evaluation | `TDbWindow(before, after)` | `Evaluate()` fills a sliding window around the cursor; only that window is in memory — good for paged UIs |

After an `Evaluate()` the cursor lands on the first row of the new block, so iterate with `do { … } while (view.NextL());` inside an outer `while (view.Unevaluated()) { view.Evaluate(); … }` loop. `Evaluate(TRequestStatus&)` is asynchronous.

## 20.8 Shared access through the DBMS server

```cpp
RDbs dbs;
User::LeaveIfError(dbs.Connect());
CleanupClosePushL(dbs);
RDbNamedDatabase db;
User::LeaveIfError(db.Open(dbs, KDbName));       // existing database
CleanupClosePushL(db);
// identical API from here
CleanupStack::PopAndDestroy(2, &dbs);
```

* Creating through `RDbs` requires a **security-policy format string** `"SECURE[UID]"`; the policy file `<UID>.spd` lives in the DBMS server's private `\policy\` directory and defines which capabilities/SIDs may read, write or change the schema. Third parties can't add policy files, so in practice you create databases directly and only *open* them through `RDbs` for sharing.
* **Secure shared databases** live in the DBMS server's private directory (`c:\private\100012a5\`).

## 20.9 Transactions and locking

* Locks are **database-wide**: many read locks, or one write lock.
* `db.Begin()` starts a transaction (read lock); the first `InsertL()`/`UpdateL()` escalates to a write lock (released back to read on `PutL()`/`Cancel()`); `db.Commit()` or `db.Rollback()` ends it. `db.InTransaction()` tells you whether one is active.
* Outside an explicit transaction each insert/update (and each DML `Execute`) runs in its **own automatic transaction** — slow for bulk work. **Wrap batches in one transaction**:

```cpp
User::LeaveIfError(db.Begin());
for (TInt i = 0; i < 1000; ++i)
    {
    tbl.InsertL();
    tbl.PutL();
    }
User::LeaveIfError(db.Commit());     // on leave: db.Rollback() (e.g. via a TCleanupItem)
```

* Nested transactions → **panic**. Reading outside a transaction while another client writes may return partially updated data.
* After a crash during a write, the database may be marked damaged: `db.IsDamaged()` → `db.Recover()`. `db.Compact()` reclaims space; `db.UpdateStats()` refreshes index statistics.

## 20.10 Incremental (step-wise) operations

For long operations with progress UI, `RDbIncremental` provides `AlterTable()`, `CreateIndex()`, `DropIndex()`, `DropTable()`, `Execute()` (DDL only), `Compact()`, `Recover()`, `UpdateStats()` with a step counter; `RDbUpdate` does SQL `UPDATE` step-wise (`RowCount()` reports rows changed so far).

```cpp
RDbIncremental incr;
TInt steps = 0;
User::LeaveIfError(incr.DropTable(db, KTable, steps));
CleanupClosePushL(incr);
while (steps > 0)
    {
    User::LeaveIfError(incr.Next(steps));     // also Next(TPckgBuf<TInt>&, TRequestStatus&)
    // update a progress bar
    }
CleanupStack::PopAndDestroy(&incr);           // Close() rolls back an unfinished operation
```

## 20.11 Symbian SQL in one screen

```cpp
#include <sqldb.h>            // sqldb.lib
RSqlDatabase sql;
TInt err = sql.Open(KDbName);
if (err == KErrNotFound) err = sql.Create(KDbName);
User::LeaveIfError(err);
CleanupClosePushL(sql);
User::LeaveIfError(sql.Exec(_L("CREATE TABLE IF NOT EXISTS t (id INTEGER PRIMARY KEY, name TEXT)")));

RSqlStatement st;
User::LeaveIfError(st.Prepare(sql, _L("INSERT INTO t(name) VALUES(:n)")));
CleanupClosePushL(st);
st.BindText(st.ParameterIndex(_L(":n")), _L("Alice"));
User::LeaveIfError(st.Exec());
CleanupStack::PopAndDestroy(&st);

User::LeaveIfError(st.Prepare(sql, _L("SELECT id, name FROM t")));
CleanupClosePushL(st);
while (st.Next() == KSqlAtRow)
    {
    TInt id = st.ColumnInt(0);
    TPtrC name;
    st.ColumnText(1, name);
    }
CleanupStack::PopAndDestroy(2, &sql);
```

Secure shared SQL databases are named `[UID3]dbname.db` and created with an `RSqlSecurityPolicy`.

## See also
* [Streams and stores](19-streams-stores.md)

# 4. Resource Files and Localization

> **Scope:** the Symbian resource language (`.rss`), resource IDs (`.rsg`), and localization (`.loc`, `.lNN`, `.rls`).

## 4.1 What resources are for

Resource files are compiled data files (`.rsc` / `.rNN`) that:

1. **Separate code from data** — UI definitions (menus, dialogs, softkeys, lists, strings) live in resources; controls initialise themselves from a resource ID.
2. **Register information with the system** — e.g. the app registration file that puts your icon in the menu, startup-list files for autostart.
3. **Localize** — one resource source, one compiled file per language.

Example: a two-field S60 query dialog defined in a resource…

```c
RESOURCE DIALOG r_multi_query
    {
    flags   = EAknGeneralQueryFlags;
    buttons = R_AVKON_SOFTKEYS_OK_CANCEL;
    items =
        {
        DLG_LINE
            {
            type    = EAknCtMultilineQuery;
            id      = EMultiQueryFirst;
            control = AVKON_DATA_QUERY
                {
                layout  = EMultiDataFirstEdwin;
                label   = "Enter data1:";
                control = EDWIN { maxlength = 50; default_case = EAknEditorTextCase; };
                };
            },
        DLG_LINE
            {
            type    = EAknCtMultilineQuery;
            id      = EMultiQuerySecond;
            control = AVKON_DATA_QUERY
                {
                layout  = EMultiDataSecondEdwin;
                label   = "Enter data2:";
                control = EDWIN { maxlength = 50; default_case = EAknEditorTextCase; };
                };
            }
        };
    }
```

…is used from C++ in three lines:

```cpp
TBuf<50> data1;
TBuf<50> data2;
CAknMultiLineDataQueryDialog* dlg = CAknMultiLineDataQueryDialog::NewL(data1, data2);
if (dlg->ExecuteLD(R_MULTI_QUERY))   // "D" = dialog deletes itself
    {
    // user pressed OK
    }
```

## 4.2 File types

| Extension | Role |
|-----------|------|
| `.rss` | Resource source file. Compiled by `rcomp` (via the build) to `.rsc` / `.rNN`. |
| `.rssi` | Fragment `#include`d into an `.rss`. Include fragments **at the end** of the `.rss`, since resource order can matter. |
| `.rh` | Resource **header**: `STRUCT` declarations (`#include <eikon.rh>`, `<avkon.rh>`, `<appinfo.rh>`). |
| `.hrh` | Constants/enums shared with C++ (`#include`d by both). |
| `.rsg` | **Generated** `#define`s of resource IDs; `#include` it in C++ (`#include <myapp.rsg>`). Produced when the MMP `START RESOURCE` block has `HEADER`; written to `\epoc32\include\`. |
| `.loc` | Localization switch: includes the right string file per `LANGUAGE_NN`. |
| `.lNN` | (S60 style) `#define`d strings for language NN. |
| `.rls` | (UIQ style, officially recommended) `rls_string` definitions. |

## 4.3 Declaring structures (`STRUCT`)

Usually you only use system structures, but you can define your own:

```c
STRUCT USER_INFO [BYTE | WORD]     // optional length prefix
    {
    BUF<20> name = "Unknown";      // default value
    BYTE    age;                   // defaults to 0
    STRUCT  friends[];             // array of any STRUCT (by convention USER_INFO)
    DOUBLE  loc[2] = {53.2091, 50.2854};
    }
```

* Struct names: **upper case**, can't start with a digit or be a type name.
* Member syntax: `[LEN [BYTE]] <type> <name> [(<maxlen>)] [[<arraysize>]] [= default];`
* Types (upper case): `BYTE` (1 byte), `WORD` (2), `LONG` (4), `DOUBLE` (8-byte float), `BUF` (Unicode string), `BUF<n>`, `BUF8` (8-bit string), `TEXT` (zero-terminated, deprecated), `LTEXT` (Unicode with length byte — most common for strings), `SRLINK` (self-link, filled in by compiler), `LINK` (16-bit ID of another resource), `LLINK` (32-bit ID of another resource), `STRUCT` (nested resource of any struct — **not type-checked**).
* Integer types accept signed and unsigned values.

## 4.4 Declaring resources (`RESOURCE`)

```c
NAME USER                       // 1–4 upper-case letters; must come first

#include <eikon.rh>
#include "users.rh"

RESOURCE RSS_SIGNATURE { }      // conventional first resource
RESOURCE TBUF { buf = ""; }     // conventional second (document name)

RESOURCE USER_INFO myresource1 { }        // all defaults

RESOURCE USER_INFO myresource2
    {
    name    = "Aleksandr";
    age     = 25;
    friends =
        {
        USER_INFO { name = "Viktor"; age = 18; loc = {56.3127, 44.0174}; },
        USER_INFO { name = "Oleg";   age = 27; loc = {55.0293, 82.9677}; }
        };
    }
```

* Resource names are **lower case**; their generated IDs are **upper case** (`MYRESOURCE2`).
* Initialising a field not in the struct is a compile error; omitted fields take the struct default (empty/0).
* Only named resources get IDs; inline (anonymous) ones don't.
* `#define` and `enum` work as in C/C++:

```c
#define max_username 20
enum TMyControls { EMyEdit1 = 1, EMyEdit2 };
```

### Resource IDs and `NAME`
Each named resource's ID = its index in the file, **prefixed by a 20-bit file ID** derived from `NAME`. With `NAME USER`, the generated `.rsg` is:

```c
#define MYRESOURCE1 0x68553001
#define MYRESOURCE2 0x68553002
```

`NAME` prevents ID clashes between several resource files loaded by one application, and doubles as the file's UID3 by default.

### Other statements
* `CHARACTER_SET UTF8` (default `CP1252`) — required in any file with non-Latin text.
* `UID2` / `UID3` — set file UIDs explicitly (UID1 is always `0x101F4A6B`), e.g. in a registration file:

```c
UID2 KUidAppRegistrationResourceFile   // from appinfo.rh
UID3 0xE1234567                        // = the application's UID3
```

## 4.5 Localization

At start-up, the GUI framework loads the application's main resource file: `name.rsc` if it exists, otherwise `name.rNN` for the current phone language (NN from `TLanguage` in `e32const.h`), falling back to the closest available language. (`BaflUtils::NearestLanguageFile()` does this for your own files.)

Steps:

1. Put every translatable string in a per-language file.
2. Declare languages in the MMP:

```text
SOURCEPATH ..\data
START RESOURCE users.rss
HEADER
LANG 01 03 16            // English, German, Russian -> users.r01, users.r03, users.r16
END
```

   The resource is compiled once per language with `LANGUAGE_NN` defined.

3. Replace literals with symbols and include a `.loc` switch file:

```c
// users.loc
#ifdef LANGUAGE_01
#include "users.l01"
#endif
#ifdef LANGUAGE_03
#include "users.l03"
#endif
#ifdef LANGUAGE_16
#include "users.l16"
#endif
```

```c
// users.l16 — UTF-8 *without BOM*
CHARACTER_SET UTF8
#define STR_name_def "Неизвестно"
#define STR_name1    "Александр"
```

   Or the `.rls` style (cannot accidentally be included from C++; no `#define`/`#include` allowed inside):

```c
// users_16.rls
rls_string STR_name_def "Неизвестно"
rls_string STR_name1    "Александр"
```

4. Use the symbols: `name = STR_name1;`
5. Ship all `.rNN` files in the SIS, typically in a language-selection block of the `.pkg` (see [SDK and build](05-sdk-and-build.md)).

Common language codes: `01` English (UK), `02` French, `03` German, `04` Spanish, `05` Italian, `06` Swedish, `07` Danish, `08` Norwegian, `09` Finnish, `10` American English, `13` Portuguese, `14` Turkish, `16` Russian, `17` Hungarian, `18` Dutch, `25` Czech, `27` Polish, `29` Taiwan Chinese, `31` PRC Chinese, `32` Japanese, `37` Arabic, `54` Greek, `57` Hebrew, `93` Ukrainian. `SC` = "no language / default".

## 4.6 Reading resources from code

The GUI framework opens your main resource file automatically (`iCoeEnv`). Common calls:

```cpp
// Load a string resource (RESOURCE TBUF r_my_text { buf = "..."; })
HBufC* text = StringLoader::LoadLC(R_MY_TEXT);        // S60, stringloader.h, commonengine.lib
// With a %U / %N parameter:
HBufC* msg = StringLoader::LoadLC(R_MY_FORMAT, name);   // "Hello %U"
CleanupStack::PopAndDestroy(2, text);

// Generic: read any resource into a buffer and parse it
TResourceReader reader;
iCoeEnv->CreateResourceReaderLC(reader, R_USER_INFO);
TPtrC name = reader.ReadTPtrC();       // LTEXT/BUF
TInt  age  = reader.ReadInt8();        // BYTE
CleanupStack::PopAndDestroy();         // reader buffer
```

Outside a GUI app, open the file yourself with `RResourceFile` (`bafl.lib`): `OpenL()`, `ConfirmSignatureL()`, `AllocReadLC(id)`.

## See also
* [GUI applications](12-applications.md) — registration and main resource files
* [Project structure](03-project-structure.md)

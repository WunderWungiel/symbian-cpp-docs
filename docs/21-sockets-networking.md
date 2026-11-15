# 21. Sockets and Networking

> **Scope:** the socket server (ESock), protocols and their properties, addresses and `RHostResolver` (DNS, Bluetooth discovery), `RSocket` (connect, listen/accept, send/receive, shutdown), `RConnection` and access points, and pointers to higher-level APIs (HTTP).
> **Headers / libs:** `es_sock.h` (`esock.lib`), `in_sock.h` (`insock.lib`), `bt_sock.h` (`bluetooth.lib`), `ir_sock.h`. **Capabilities:** `NetworkServices` for TCP/IP and SMS sockets, `LocalServices` for Bluetooth/IrDA.

## 21.1 The socket server

* **ESock** (`esock.exe`) implements Berkeley-style sockets over many transports. Protocols are **PRT plug-ins** loaded on demand, each in its own thread.
* Uses: TCP/UDP/ICMP over IPv4/IPv6, Bluetooth (L2CAP, RFCOMM, SDP, AVCTP, AVDTP, Link Manager), IrDA (TinyTP, IrMUX), SMS (SMS PRT).
* Client session: **`RSocketServ`**.

```cpp
RSocketServ ss;
User::LeaveIfError(ss.Connect());            // Connect(TUint aMessageSlots) / Connect(TSessionPref)
CleanupClosePushL(ss);
// ... RSocket / RHostResolver / RConnection sub-sessions ...
CleanupStack::PopAndDestroy(&ss);
```

| `RSocketServ` | |
|---------------|-|
| `NumProtocols(TUint&)`, `GetProtocolInfo(index, TProtocolDesc&)` (1-based), `FindProtocol(name, TProtocolDesc&)` | Enumerate protocols |
| `StartProtocol(family, sockType, protocol, status)` / `StopProtocol(...)` | Pre-load/unload a protocol (`NetworkControl`) — normally automatic |
| `InstallExtension()`, `SetExclusiveMode()` | `NetworkControl`; rare |
| `Connect(TSessionPref)` | Hint the protocol mostly used, to speed up routing |

### Protocol properties (`TProtocolDesc`)
`iAddrFamily` (e.g. `KAfInet` = IPv4, `KAfInet6`, `KBTAddrFamily`, `KIrdaAddrFamily`), `iProtocol` (e.g. `KProtocolInetTcp`, `KProtocolInetUdp`, `KProtocolInetIcmp`, `KRFCOMM`, `KL2CAP`, `KSDP`, `KBTLinkManager`, `KIrTinyTP`), `iName` (`"tcp"`, `"udp"`, `"RFCOMM"`…), `iSockType`, `iServiceInfo` flags (`KSIReliable`, `KSIConnectionLess`, …).

| Socket type | Meaning |
|-------------|---------|
| `KSockStream` | Reliable byte stream (TCP, RFCOMM) |
| `KSockDatagram` | Bounded packets; may be unreliable (UDP) |
| `KSockSeqPacket` | Reliable ordered packets (L2CAP) |
| `KSockRaw` | Raw protocol access |

| Protocol | Family | Type | Reliable | Connection |
|----------|--------|------|----------|------------|
| TCP | `KAfInet` | stream | yes | yes |
| UDP | `KAfInet` | datagram | no | no |
| ICMP | `KAfInet` | datagram | no | no |
| L2CAP | `KBTAddrFamily` | seq-packet | yes | yes |
| RFCOMM | `KBTAddrFamily` | stream | yes | yes |
| SDP | `KBTAddrFamily` | datagram | yes | yes |
| TinyTP | `KIrdaAddrFamily` | seq-packet | yes | yes |
| IrMUX | `KIrdaAddrFamily` | datagram | yes | no |

## 21.2 Addresses

`TSockAddr` (family + port) is the base; protocol-specific subclasses: **`TInetAddr`** (IPv4/IPv6 + port), `TBTSockAddr`, `TInquirySockAddr` (Bluetooth), `TIrdaSockAddr`. Pass them wherever a `TSockAddr` is expected.

```cpp
TInetAddr addr;
addr.SetAddress(INET_ADDR(192, 168, 0, 1));
addr.SetPort(80);
addr.Input(_L("77.88.21.8"));     // parse text (IPv4 or IPv6); returns error
TBuf<39> text;
addr.Output(text);                // to text
TInetAddr any(KInetAddrAny, 2000);
```

## 21.3 Name resolution: `RHostResolver`

```cpp
// DNS
RHostResolver resolver;
User::LeaveIfError(resolver.Open(ss, KAfInet, KProtocolInetUdp));   // or (ss, KAfInet, KProtocolInetTcp, connection)
CleanupClosePushL(resolver);
TNameEntry entry;                                  // TPckgBuf<TNameRecord>
User::LeaveIfError(resolver.GetByName(_L("example.com"), entry));   // async overload with TRequestStatus
TInetAddr addr(entry().iAddr);
User::LeaveIfError(resolver.GetByAddress(addr, entry));
TPtrC host = entry().iName;
TBuf<64> me;
resolver.GetHostName(me);
CleanupStack::PopAndDestroy(&resolver);
```

`resolver.Next(entry)` returns further results (e.g. multiple addresses).

```cpp
// Bluetooth device discovery (LocalServices; Bluetooth must be on, else KErrHardwareNotAvailable)
RHostResolver bt;
User::LeaveIfError(bt.Open(ss, KBTAddrFamily, KBTLinkManager));
CleanupClosePushL(bt);
TInquirySockAddr inq;
inq.SetIAC(KGIAC);                                 // general inquiry
inq.SetAction(KHostResInquiry | KHostResName);     // discover and fetch names
TNameEntry dev;
TInt err = bt.GetByAddress(inq, dev);              // first device (takes seconds — use the async version!)
while (err == KErrNone)
    {
    TBTDevAddr devAddr = static_cast<TBTSockAddr>(dev().iAddr).BTAddr();
    TPtrC devName = dev().iName;
    err = bt.Next(dev);
    }
CleanupStack::PopAndDestroy(&bt);
```

On S60 the system device-selection dialog (`RNotifier` with `KDeviceSelectionNotifierUid`) is the user-friendly alternative; service discovery uses the SDP agent API (`CSdpAgent`, `sdpagent.lib`).

## 21.4 `RSocket`

A sub-session of `RSocketServ`. Opening:

```cpp
RSocket sock;
User::LeaveIfError(sock.Open(ss, KAfInet, KSockStream, KProtocolInetTcp));
// or sock.Open(ss, _L("tcp"));  or with an RConnection: Open(ss, KAfInet, KSockStream, KProtocolInetTcp, conn)
CleanupClosePushL(sock);
```

### Client (active) connection

```cpp
TInetAddr server(addrFromDns);
server.SetPort(80);
sock.Connect(server, iStatus);       // async; CancelConnect() to cancel
SetActive();                         // in an active object
```

### Server (passive) connection

```cpp
RSocket listener;
User::LeaveIfError(listener.Open(ss, _L("tcp")));
TInetAddr local(KInetAddrAny, 2000);
User::LeaveIfError(listener.Bind(local));        // or SetLocalPort(2000) for TCP/UDP
User::LeaveIfError(listener.Listen(5));          // queue size
RSocket blank;
User::LeaveIfError(blank.Open(ss));              // "blank" socket: no protocol
listener.Accept(blank, iStatus);                 // completes when a client connects; blank is now connected
SetActive();
```

If the queue overflows (nobody calls `Accept()`), new connections are refused.

### Sending and receiving (8-bit descriptors)

| Connection-oriented | Connectionless |
|---------------------|----------------|
| `Send(des, flags, status [, TSockXfrLength&])` | `SendTo(des, addr, flags, status [, len])` |
| `Recv(des, flags, status [, len])` — stream: completes only when **des is full** (to its MaxLength) or the connection closes; datagram: one datagram (excess lost) | `RecvFrom(des, addr, flags, status [, len])` |
| **`RecvOneOrMore(des, flags, status, TSockXfrLength& len)`** — streams: completes as soon as *any* data arrives. **Use this for TCP.** | |
| `Write(des, status)` / `Read(des, status)` — `Send`/`Recv` without flags (also for connectionless sockets after `Connect()` set a default address) | |
| Cancel: `CancelSend()`, `CancelRecv()`, `CancelWrite()`, `CancelRead()`, `CancelAll()` | |

For connectionless sockets, `Connect()` doesn't connect but sets the **default destination**.

```cpp
// Active-object TCP reader skeleton
void CReader::IssueRead()
    {
    iBuffer.Zero();
    iSocket.RecvOneOrMore(iBuffer, 0, iStatus, iLen);   // iBuffer: TBuf8<512> MEMBER, iLen: TSockXfrLength MEMBER
    SetActive();
    }
void CReader::RunL()
    {
    if (iStatus == KErrEof) { iObserver.Closed(); return; }   // remote closed
    User::LeaveIfError(iStatus.Int());
    iObserver.DataReceived(iBuffer);
    IssueRead();
    }
void CReader::DoCancel() { iSocket.CancelRecv(); }
```

All socket I/O is asynchronous: keep buffers and length objects as **members** until completion; use one active object per direction (reader and writer) since an AO can only have one request outstanding.

### Closing

* Connectionless: `CancelAll()` then `Close()`.
* Connection-oriented: **`Shutdown(RSocket::TShutdown, status)`** first, then `Close()`:
  * `ENormal` — finish pending reads/writes, then disconnect;
  * `EStopInput` — abort reads, finish writes;
  * `EStopOutput` — abort writes, finish reads;
  * `EImmediate` — abort everything (completes immediately).
  * An overload sends disconnect data (`GetDisconnectData()` on the other side), where supported.

### Other `RSocket` members
`Info(TProtocolDesc&)`, `GetOpt(name, level, value)` / `SetOpt(...)` (e.g. `KSoTcpNoDelay`/`KSolInetTcp`, `KSoReuseAddr`, `KSORecvBuf`), `LocalName()`, `LocalPort()`, `RemoteName()`, `Ioctl(command, status, desc, level)` / `CancelIoctl()`, `Transfer()` / `Name()` (hand a socket to another session/process).

## 21.5 Connections and access points: `RConnection`

TCP/UDP can run over GPRS/3G, WLAN, Bluetooth PAN, or the PC's network (emulator). Opening a socket without an active connection triggers an implicit connection and usually an **access-point selection dialog**. To control it, start a connection explicitly and pass it to sockets and resolvers:

```cpp
RConnection conn;
User::LeaveIfError(conn.Open(ss));
CleanupClosePushL(conn);

TCommDbConnPref prefs;                               // commdbconnpref.h, commdb.lib
prefs.SetIapId(iapId);                               // a known access point…
prefs.SetDialogPreference(ECommDbDialogPrefDoNotPrompt);
// …or ECommDbDialogPrefPrompt to let the user choose
conn.Start(prefs, iStatus);                          // async; Start() with no prefs = default/ask
SetActive();
// later: sock.Open(ss, KAfInet, KSockStream, KProtocolInetTcp, conn);
//        resolver.Open(ss, KAfInet, KProtocolInetUdp, conn);
```

* `Attach()` to an existing connection, `ProgressNotification()` for state changes, `Stop()` to tear down (`NetworkControl` for others' connections), `EnumerateConnections()`/`GetConnectionInfo()`.
* Access points are stored in **CommsDat** (CommDB). Enumerate IAPs with `CCommsDatabase`/`CCommsDbTableView` (legacy, `commdb.lib`) or CommsDat (`commsdat.lib`); S60 offers `CApSelect`/`CApUtils` and on 3rd FP2+ the **destination/SNAP** APIs (`TConnSnapPref`, `RCmManager`).
* Keep one `RConnection` open while you work and reuse it — repeated implicit connections show repeated dialogs.

## 21.6 Higher-level networking

| Need | API |
|------|-----|
| HTTP/HTTPS client | **HTTP framework** `RHTTPSession`, `RHTTPTransaction`, `MHTTPTransactionCallback` (`http.lib`, `ecom.lib`, `bafl.lib`, `inetprotutil.lib`). Set the connection via the session's connection-info properties (`HTTP::EHttpSocketServ`, `HTTP::EHttpSocketConnection`). |
| TLS sockets | `CSecureSocket` (`securesocket.lib`) over a connected `RSocket` |
| Open a URL in the browser | `CBrowserLauncher` (S60) or start the browser via `TApaTaskList`/`RApaLsSession` with a URL parameter |
| SMS | SMS sockets (`KSMSAddrFamily`, `smsu.lib`; `ReadUserData`/`WriteUserData`/`NetworkServices`) or Messaging MTMs (`CSmsClientMtm`); `CSendUi`/`CSendAs` for "send via" UI |
| Telephony | `CTelephony` (ETel 3rd-party API, `etel3rdparty.lib`): calls, IMEI/IMSI, signal, battery, network info — various capabilities |
| POSIX sockets | Open C (`libc`) `socket()`/`connect()` — handy for ported code |
| Qt | `QNetworkAccessManager`, `QTcpSocket` (Qt 4.6+ for Symbian) |

## 21.7 Pitfalls
* Missing `NetworkServices`/`LocalServices` → `KErrPermissionDenied`.
* Using `Recv()` on TCP with a large buffer "hangs" until it's full — use `RecvOneOrMore()`.
* Buffers passed to async calls going out of scope → corrupted data / `KERN-EXEC 3`.
* Not closing `RConnection` keeps the data connection (and battery drain) alive; not reusing it causes repeated AP dialogs.
* Synchronous `User::WaitForRequest()` on network operations freezes the UI — use active objects.
* In the emulator, networking needs the PC's connection configured (*Tools → Preferences → Network*, or WinTAP/Ethernet plug-in depending on SDK).

## See also
* [Active objects](11-active-objects.md)
* [Platform security — capabilities](02-platform-security.md#22-capabilities)

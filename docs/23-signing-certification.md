# 23. Signing and Certification (Symbian Signed)

> **Scope:** how applications were certified 2005–2013 (self-signed, Open Signed Online/Offline, Express Signed, Certified Signed, Publisher IDs, UID allocation, test criteria), and what to do now that Symbian Signed no longer exists.
> **Background:** see [Platform security](02-platform-security.md#25-installation-and-signing) for why signing matters.

## 23.1 Capability groups vs. signing routes

| Capabilities | Self-signed | Open Signed Online | Open Signed Offline | Express Signed | Certified Signed |
|--------------|:-----------:|:------------------:|:-------------------:|:--------------:|:----------------:|
| User (`LocalServices`, `Location`*, `NetworkServices`, `ReadUserData`, `UserEnvironment`, `WriteUserData`) | yes | yes | yes | yes | yes |
| System (`PowerMgmt`, `ProtServ`, `ReadDeviceData`, `SurroundingsDD`, `SwEvent`, `TrustedUI`, `WriteDeviceData`) | no | yes | yes | yes | yes |
| Restricted (`CommDD`, `DiskAdmin`, `NetworkControl`, `MultimediaDD`) | no | no | yes | no | yes |
| Manufacturer (`AllFiles`, `DRM`, `TCB`) | no | no | with manufacturer approval | no | with manufacturer approval |

\* `Location` was a system capability on S60 3rd Ed./FP1.

| | Open Signed Online | Open Signed Offline | Express Signed | Certified Signed |
|-|-------------------|--------------------|----------------|------------------|
| Publisher ID needed | no | yes | yes | yes |
| Cost | free | free | paid (≈ US$20 per submission, "Content ID") | paid (test house, from ≈ €160) |
| Independent testing | no | no | no (self-test, random audits) | **yes** |
| IMEI-restricted | yes (one IMEI) | yes (up to 1000+ IMEIs) | no | no |
| For commercial distribution | no | no | yes | yes ("for Symbian OS" logo) |
| Validity | 3 years | 3 years | 10 years | 10 years |
| UIDs | test range `0xE…` (or your own protected UIDs) | protected range | protected | protected |

Self-signed packages install with a warning ("untrusted"); many stores refused them, so developers often paid for Express Signed even when self-signing would have worked. Developer certificates also warn at install time.

## 23.2 Publisher ID

* An **ACS Publisher ID** — a code-signing certificate (1 year) from a CA (originally VeriSign, later **TC TrustCenter**; ≈ US$200). Only for legal entities (companies or registered sole traders); identity documents were checked.
* It identifies the author (abuse → black-listing); it does **not** grant capabilities by itself.
* Bought through the browser (the key pair is generated in and bound to that browser); export as PKCS#12 `.pfx`, then convert to `.key` (PKCS#8) + `.cer` (X.509) with **TC-ConvertP12** (`tcp12p8.bat file.pfx password my.key my.cer`) for use with `signsis`.

## 23.3 Open Signed Online

Web form on symbiansigned.com: phone **IMEI** (dial `*#06#`), e-mail, unsigned SIS, captcha, licence. The signed SIS arrived by e-mail; valid for that one device for 3 years. No restricted/manufacturer capabilities; UIDs from `0xE0000000`–`0xEFFFFFFF` (unless you had reserved protected UIDs on an account with the same e-mail).

## 23.4 Open Signed Offline

1. Create a certificate request (`.csr`, PKCS#10) with **DevCertRequest**: choose output file → your Publisher ID `.cer`/`.key` + password → confirm owner → list IMEIs (≤ 1000) and capabilities (tip: request all except manufacturer ones — it's free and reusable) → finish.
2. Upload the CSR in *My Symbian Signed → Open Signed → Request*; download the resulting DevCert `.cer`.
3. Sign with that `.cer` + your Publisher ID `.key`: `signsis -s app.sis app.sisx devcert.cer publisher.key password`. Reuse until expiry.

## 23.5 UID reservation
Protected UIDs (`0x2…`) were requested free from *My Symbian Signed → UIDs → Request* (≤ 20 per day; more on request), recorded against your account/company. Required for Express/Certified Signed. Also: package UIDs (PID) and, if needed, VIDs.

## 23.6 Symbian Signed Test Criteria (summary)

Not a usability or content review — robustness and policy compliance on the devices you declared. Version 3.0.x / 4.x criteria included:

**Universal (UNI) tests**
* Installs, uninstalls, re-installs cleanly — **including to a memory card** — and leaves **no files behind** after uninstall (keep generated data in `\private\<SID>\`).
* Backup/restore works if supported (register with the backup server via a `backup_registration.xml` in the private directory).
* Files are created only in appropriate locations.
* Survives stress: low memory (OOM handling), low disk, sudden power-off, rapid task switching, many instances of events.
* Handles system events correctly and closes properly from the **task manager** (respond to `EEikCmdExit`/`EAknCmdExit`); no unexpected panics.
* Correct display in all supported **resolutions and orientations** (layout switching).
* Autostart at boot must be user-configurable (a way to turn it off).
* Name, version, vendor and UIDs consistent between PKG and the app; correct platform/product UIDs in the PKG.

**Capability (CAP) tests**
* Multimedia apps must not block or disturb voice calls.
* Call-diverting apps must have a UI.
* VoIP apps must show the manufacturer's disclaimer and not interfere with GSM functions, including emergency calls.
* Each declared capability must be justified by APIs actually used.

## 23.7 Express Signed and Certified Signed submissions

* **Express Signed**: buy *Content IDs*; upload a ZIP with the Publisher-ID-signed SIS, the PKG, a short TXT description (and registration keys for shareware); fill in application data, the device model used for testing, **your own test results** per criterion (pass/fail/exception code), and a **justification for each capability** (which API needs it). Signing completes within minutes; random audits punish false declarations (from Express Signed suspension to Publisher ID revocation).
* **Certified Signed**: same wizard, choose a **test house** (e.g. NSTL, MphasiS, Sogeti High Tech), pay it directly; testing took ~3–4 days; result *Accepted* or *Rejected* with a report. Rejected → fix and resubmit (and pay again).

## 23.8 Today (post-2014)

Symbian Signed, Open Signed Online/Offline and the Nokia Store are **gone**; no new trusted certificates can be obtained. What remains:

1. **Self-signing** with `makekeys`/`signsis` (or Carbide's default) — works on every device for **user capabilities only**, and requires *Settings → Application manager → Software installation: All* and *Online certificate check: Off* on the phone.
2. **Existing developer certificates** keep working only for the IMEIs they list, and only while valid (the device clock matters — some people set the phone date back to install with an expired DevCert).
3. **Modified devices**: the hobbyist community patches the installer's capability check (e.g. via ROM-patcher tools and "installserver" patches) on their *own* phones so self-signed packages can use any capability. This is outside the official security model; do it only on devices you own and understand that such packages won't install on stock phones.
4. **Choose unprotected UIDs** (`0xE…` for experiments, `0xA…`-range for self-signed releases) — protected `0x2…` UIDs are rejected in self-signed packages on stock devices.

## See also
* [Platform security](02-platform-security.md)
* [Packaging and signing tools](05-sdk-and-build.md#58-makesis-makekeys-signsis)

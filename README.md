# RDP Shadow

A minimal Windows desktop tool for shadowing RDP (Remote Desktop) sessions — built with WPF and Fluent Design acrylic UI.

![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)
![.NET](https://img.shields.io/badge/.NET-8.0-512BD4)
![Single file](https://img.shields.io/badge/build-single--file%20exe-success)

## What it does

Lists the RDP sessions on a local or remote Windows machine and lets you shadow any of them with one click. Under the hood it wraps `query.exe session` for enumeration and `mstsc /shadow` for the actual shadowing — but with a clean acrylic UI instead of a command prompt.

## Features

- **Acrylic Fluent Design** — real DWM backdrop, auto light/dark theme following Windows
- **Async session query** — UI never freezes, even on slow/unreachable hosts
- **Localization-aware** — correctly colors session state on non-English Windows (pt-BR, es, fr, de, ru, sv)
- **Control toggle** — switch between view-only and full keyboard/mouse takeover (`/control`)
- **Single-file self-contained** — no .NET runtime needed on the target machine

## Requirements

- Windows 10 / 11 (x64)
- **Administrator privileges** (required by `query session` and `mstsc /shadow`)
- For remote shadowing: admin credentials on the target, appropriate firewall rules, and registry policy `HKLM\SOFTWARE\Policies\Microsoft\Windows NT\Terminal Services\Shadow` set on the target

## Download

Grab the latest `RdpShadow.exe` from the [Releases page](../../releases/latest). Single file, no installer, no dependencies.

## Usage

1. Launch `RdpShadow.exe` (it will elevate via UAC).
2. Type a hostname or IP in the **Server** field (leave blank for localhost).
3. Press **Refresh** — sessions appear in the list.
4. Select the session you want to shadow.
5. Toggle **Control** on if you need full keyboard/mouse, off for view-only.
6. Click **Shadow Session**.

## Building from source

```bash
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o publish/
```

Output: `publish/RdpShadow.exe` (~70 MB, everything bundled).

## Tech stack

- .NET 8.0 (WPF)
- [Wpf-Ui](https://github.com/lepoco/wpfui) 3.0.5 — Fluent Design controls
- Windows built-ins: `query.exe`, `mstsc.exe`

## License

MIT

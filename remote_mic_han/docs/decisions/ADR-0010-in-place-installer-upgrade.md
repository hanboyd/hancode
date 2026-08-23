# ADR-0010: In-place installer upgrade with preserved user data

- Status: accepted
- Date: 2026-08-23

## Context

Users should be able to install a newer Remote Mic build without first
uninstalling or deleting the older installed version. The application's runtime
data currently shares the per-user `%LOCALAPPDATA%\RemoteMic\RC003` root with the
installed program, so deleting that root during upgrade would also erase device
selection, key mappings, statistics and logs.

## Decision

The Inno Setup installer keeps its existing stable `AppId` and per-user install
root. Versioned program files are placed in the dedicated `{app}\app` payload
directory. Before copying a newer payload, the installer stops processes whose
executables live under `{app}`, deletes only `{app}\app`, and then writes the new
payload. A one-time targeted migration removes the previous flat
`{app}\RemoteMicRC003.exe` and `{app}\_internal` PyInstaller layout.

Runtime-generated files remain at the install root and are outside the deletion
scope. In particular, upgrades preserve `config.json`, `key_bindings.json`,
`usage-statistics.json` and `logs`. Broad deletion such as `{app}\*` is forbidden.
The stable `AppId` means Windows retains one installed-app/uninstall entry.

Portable ZIP files remain manually replaceable artifacts and must not be
described as supporting this installer upgrade contract.

## Consequences

- Running a newer installer replaces the older installed application without a
  separate uninstall step.
- Old program files cannot accumulate inside the dedicated payload directory.
- Settings, mappings, statistics and logs survive an upgrade.
- Any future installer layout change must include a targeted migration that does
  not broaden deletion to the runtime-data root.

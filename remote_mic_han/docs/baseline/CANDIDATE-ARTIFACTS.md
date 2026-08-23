# RC003 unsigned candidate artifacts

Date: 2026-08-22

This record covers the first hardware-independent packaging candidate. It is not a hardware acceptance record and does not claim successful BLE, HID, microphone, Typeless, or Qianwen operation.

## Deliverables

| Artifact | Bytes | SHA-256 | Signature |
| --- | ---: | --- | --- |
| `artifacts/RemoteMicRC003-0.1.0-candidate-portable-unsigned.zip` | 118465291 | `E65A6E00DF3B73BC63E9620821703FF24120059F9ADDF1B6B7AECECDDCF3EB33` | Not applicable |
| `artifacts/RemoteMicRC003Setup-0.1.0-candidate-unsigned.exe` | 73193309 | `5378DFDF097CCF2D61CC3CF9C5AC87CFC4B41B919E424BBEAFF60F4CD129F355` | `NotSigned` |

Each artifact has a neighboring `.sha256` sidecar. These are local candidates under the ignored `artifacts/` directory and are not committed source files.

## Verification completed

- Full imported baseline suite: 941 passed, 7 skipped, 0 failed.
- Root C++ foundation: CTest 1/1 passed.
- Public-boundary scan passed before packaging.
- PyInstaller directory candidate built successfully.
- Frozen executable `--dry-run` returned exit code 0.
- Portable ZIP contains the executable, required runtime files, pinned VB-CABLE archive, notices, and internal hashes; it contains neither `.venv` nor Python source files.
- A fresh extraction of the portable ZIP ran `RemoteMicRC003.exe --dry-run` and returned exit code 0.
- Installer compiled successfully with Inno Setup 6.7.3 and is intentionally unsigned.
- No Frida Gadget binary is bundled.
- Source and frozen settings windows were launched visibly on Windows; the Mac-inspired side rail, two-column connection page, RC003 image, and all four page layouts rendered.
- Repeated frozen settings launches left exactly one visible settings window.
- The 900×680 OpenDesign UI reference passed independent visual verification without page-level clipping or scrollbars.
- The rebuilt installer was overwrite-installed through its real UI with the desktop shortcut enabled.
- The installed executable SHA-256 matched the rebuilt executable: `81D22701E7EF7F76DFABE4E24CC0BE5D027299BE07720DC17963C06D0FA08DE8`.
- The installed redesigned connection page rendered visibly; repeated installed settings launches left one window and one process.
- The refined frozen executable embeds the new blue microphone product icon and opens a responsive settings window.
- The refined audio menu collapses duplicate host-API rows, hides system-default aliases/backend names, and reduced this machine's list from 16 rows to 8 logical devices.

## Deferred and residual checks

- Physical RC003 discovery, BLE/ATVV streaming, HID behavior, real audio routing, and target-application acceptance remain deferred until the remote is available.
- The refined installer has not yet overwritten the currently installed candidate; installation and installed-UI inspection await explicit confirmation.
- Uninstall/residue behavior remains pending explicit authorization.

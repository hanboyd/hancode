# RC003 first usable unsigned candidate artifacts

Date: 2026-08-23

This record covers the first user-accepted Windows candidate and the local
artifacts rebuilt from the current working tree. The artifacts remain unsigned.

## Deliverables

| Artifact | Bytes | SHA-256 | Signature |
| --- | ---: | --- | --- |
| `artifacts/RemoteMicRC003-0.1.0-candidate-portable-unsigned.zip` | 125029599 | `82A387E264D03A5E6FE1D96D488624D1D28694B3B2AD77DD0CDC53CC0014A61C` | Not applicable |
| `artifacts/RemoteMicRC003Setup-0.1.0-candidate-unsigned.exe` | 82211286 | `29940A4178D4BA2FB27E0480FF23E4A9E5206422FBCCC17BC82456D724DBEE6B` | `NotSigned` |

Each artifact has a neighboring `.sha256` sidecar. These are local deliverables
under the ignored `artifacts/` directory and are not committed source files.

## Verification completed

- Full baseline regression: 1,041 tests passed with 7 environment skips and 0
  failures after the installer-layout and packaging-script changes.
- Public-boundary scan passed with 235 files scanned.
- PyInstaller directory candidate built successfully; the frozen executable
  passed `--dry-run`.
- The portable ZIP was extracted into a fresh directory and the extracted
  executable passed `--dry-run`.
- Both SHA-256 sidecars match their generated artifact exactly.
- Installer compiled successfully with Inno Setup 6.7.3 and its Authenticode
  status is intentionally `NotSigned`.
- ADR-0010's upgrade structure is compiled and covered by artifact tests: one
  stable AppId, a dedicated replaceable `{app}\app` payload, targeted migration
  from the previous flat PyInstaller layout, and no broad deletion of the
  runtime-data root.
- User-driven real-device acceptance established Typeless and Qianwen as usable
  with the RC003 and virtual microphone before this packaging run. Their known
  intermittent foreground text-commit behavior remains deferred and is not
  represented as fixed by packaging.

## Deferred and residual checks

- This newly generated installer has not been run over the user's currently
  installed candidate, because packaging did not authorize disturbing that live
  installation. The next real update should verify that the old executable is
  replaced, settings/mappings/statistics/logs remain, and Windows shows one
  installed-app entry.
- Uninstall/residue validation still requires explicit user authorization.
- The portable ZIP does not implement in-place upgrade; it is a manually
  replaceable fallback artifact.

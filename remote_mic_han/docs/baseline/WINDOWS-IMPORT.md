# Windows Product Baseline Import

## Source role

- Active development repository: `remote_mic_han`
- Read-only GitHub download: the separate `remote-mic-windows-port` folder supplied by the user
- Imported snapshot directory: `windows-remote-mic-app-271ed7947eec19c4c691ed3ba97f338461be8051`
- Source revision encoded by the downloaded folder: `271ed7947eec19c4c691ed3ba97f338461be8051`
- Downloaded archive SHA-256: `362606494E3535DA24E7211B49772B1D1E52F05B6C99CAE03198F4D01F52800B`
- Import date: 2026-08-22

The downloaded folder has no `.git` metadata, so the folder name and archive hash are the available provenance evidence. The source download itself was not modified.

## Imported

- `apps/windows/rc003/` (113 source, test, UI, and build files before creating `.venv`)
- `device-profiles/`
- `Resources/`
- `.github/workflows/windows-rc003-ci.yml`
- `docs/screenshots/`
- `LICENSE.md`, `COPYRIGHT.md`, and `THIRD_PARTY_NOTICES.md`

## Deliberately not imported

- macOS/Swift source
- the old downloaded installer and ZIP archive
- `.codex` state from the downloaded repository
- generated environments, PyInstaller output, caches, logs, device identities, or voice recordings

## Integrity anchors

| File | SHA-256 immediately after import |
|---|---|
| `apps/windows/rc003/src/ovb_rc003/atvv_protocol.py` | `B857B77C5482DE66E37AFC06FF9AEC4AC051492BAD5A5CDA03485C6529B09F94` |
| `apps/windows/rc003/src/ovb_rc003/atvv_session.py` | `B8CE884FCFAF15B4DC7F4296A3030D4EA72171E2E6C7725A9E902203D122E631` |
| `apps/windows/rc003/pyproject.toml` | `802FB9A49DFED76B4F1F9E15E4058E0F35776244DF97FA7834CED73DDEECBF17` |
| `LICENSE.md` | `3972DC9744F6499F0F9B2DBF76696F2AE7AD8AF9B23DDE66D6AF86C9DFB36986` |
| `THIRD_PARTY_NOTICES.md` | `8BF74060007A59ABE659A9D8B18D01944837D2E8198B0A7A1CF28A5E0CF10D76` |

## Known metadata conflict

The downloaded root README says the candidate previously passed real RC003 acceptance, while the package metadata and CLI text still say it has not been real-device verified. For the active repository, the conservative status wins: historical acceptance is recorded as source history, while current hardware validation remains `deferred` until repeated with the user's device.

## License

The imported implementation is GPL-3.0-only. Its license, copyright, attribution, and third-party notices must stay with the code and release artifacts.

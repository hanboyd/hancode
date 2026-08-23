# Windows UI validation

## Scope

- Target: `0.1.0-candidate`, Mac-inspired Windows QML redesign.
- Pages: connection and voice, button mapping, permissions and system, checks and repairs.
- This test does not prove RC003, BLE, HID, audio routing, Typeless, Qianwen, or installer trust.

## Preparation

1. Use a normal Windows desktop session at the current display scale.
2. Keep the physical RC003 disconnected if it is unavailable.
3. Close any existing Remote Mic settings window.
4. For source checks, launch `apps/windows/rc003/.venv/Scripts/pythonw.exe src/launcher.py --settings` from the app directory.
5. For candidate checks, launch `apps/windows/rc003/dist/RemoteMicRC003/RemoteMicRC003.exe --settings`.

## Cases

### 1. Window and navigation

1. Open settings.
2. Confirm the window starts near 1020×740 and remains usable down to 900×640.
3. Click 连接、按键映射、权限、检查 in order.

Expected:

- A narrow left navigation rail remains visible.
- The selected item uses the Windows accent color.
- Every page has one large title and no duplicated page title.
- Chinese labels remain readable; no text is intentionally reduced below approximately 12 pt.

Failure:

- Top tabs return, navigation is clipped, a page title is duplicated, or controls overlap.

### 2. Connection and voice

1. Open 连接.
2. Confirm the RC003 photo appears in the left device card.
3. Confirm output device, hotkey, trigger method, bridge status, save/start, and log actions remain reachable in the right card.

Expected:

- Device and voice settings form a two-column layout at the default size.
- Missing hardware is described as pending rather than connected or passed.
- The window has no horizontal scrollbar.

Failure:

- The photo is distorted or missing, a primary action is clipped, or unavailable hardware is reported as successful.

### 3. Button mapping

1. Open 按键映射.
2. Click a remote hotspot and a mapping card.
3. Scroll the mapping list.

Expected:

- The remote keeps its original aspect ratio.
- The first mapping row is fully visible on entry.
- Selection remains synchronized and mapping controls do not overlap.

Failure:

- The list opens midway through a clipped row, the remote is distorted, or controls overlap.

### 4. Permissions and diagnostics

1. Open 权限 and confirm each Windows settings action is visible.
2. Open 检查 and wait for diagnostics.
3. Scroll through every diagnostics group.

Expected:

- Cards remain separated and readable.
- Actual failed, passed, manual, and unavailable states retain distinct semantic colors.
- Hardware absence remains a failed/deferred prerequisite, not a successful connection.

Failure:

- A diagnostic result is hidden, a running process is presented as end-to-end success, or a system action is unreachable.

### 5. Settings single instance

1. Keep settings open.
2. Launch the same settings entry again.

Expected: one settings window remains and the existing window is restored or activated.

Failure: two settings windows appear, the second process blocks on a modal dialog, or the existing window cannot be found.

### 6. Product icon and audio menu

1. Confirm the same blue microphone icon appears in the title bar, taskbar, Start shortcut and installer.
2. Open the voice output menu.

Expected:

- One row appears per logical playback device.
- MME, DirectSound, WASAPI and WDM-KS are not shown to the user.
- Microsoft Sound Mapper and Primary Sound Driver aliases are hidden when concrete devices exist.

Failure: a generic app icon appears, the icon changes between surfaces, or the same named device is repeated by backend.

## Evidence and logs

- Application log: `%LOCALAPPDATA%\RemoteMic\logs\runtime.log`.
- OpenDesign reference: `opendesign/mockups/remote-mic-windows/mac-inspired-settings.html`.
- Independent 900×680 reference screenshot: `artifacts/ui-qa/mac-inspired-settings-900x680-final.png` (ignored local evidence).

## Current boundary

- Automated QML and complete baseline suites: passed.
- Source-rendered and rebuilt frozen GUI page inspection: passed.
- Rebuilt installer overwrite/install, installed UI, executable identity, and settings single-instance behavior: passed.
- Uninstall residue: pending explicit authorization; the accepted candidate remains installed.
- Physical RC003 and target-application acceptance: deferred because the device is unavailable.
- Refined icon/audio-menu installer: built and automated checks passed; overwrite installation awaits explicit confirmation.

# ADR-0019: Retain the accepted QML UI and move state into C++

- Status: Accepted
- Date: 2026-09-01

## Decision

Keep the existing PySide6/Qt Quick rendering client and the accepted QML
interface byte-for-byte. Do not redesign the window and do not translate it to
WinUI. Move UI business state behind the existing QML singleton interface into
the native extension page by page.

The accepted interface is the card-based ten-file QML set published by commit
`19a0004` and shipped in the previous installed build. The initial Phase 9 copy
manifest accidentally pinned the earlier seven-file import UI instead. That
mistake was corrected on 2026-09-03 after direct user comparison; the three
shared `components/*.qml` files are now included in the copy contract.

The first native boundary is `UiSettingsState`: it owns voice-mode/hotkey
pairing, endpoint selection, device selection and selected-button state.
PySide6 continues to own QObject signals, QAbstractListModel notifications,
the QML engine and Windows system-operation adapters.

This choice gives an exact visual copy without introducing a second Qt runtime.
The installed PySide6 wheel supplies the tested Qt Quick runtime but not the Qt
C++ development/import libraries needed for a separate executable. Downloading
another Qt SDK would increase build and packaging scope without changing the
accepted UI.

## Copy contract

`tests/fixtures/phase9-qml-copy-contract.json` records SHA-256 for every QML
file recursively. The Phase 9 test fails if a page or shared component is
added, removed or visually edited.
Existing offscreen QML-load, geometry, accessibility, interaction and settings
read/write tests remain the behavioral contract.

## Consequences

- Users see the same window, pages, text, geometry and navigation.
- Native state can be tested without Qt, hardware or a display server.
- Source runs without `_C.pyd` retain a matching Python state fallback.
- Diagnostics workers and OS dialogs remain Python adapters for now; they are
  not duplicated in C++ and are still covered by their existing shutdown tests.
- Packaging remains outside this decision and is still deferred.

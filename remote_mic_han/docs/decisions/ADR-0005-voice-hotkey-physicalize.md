# ADR-0005: Voice Hotkey Physicalization

- Status: accepted
- Date: 2026-08-23
- Related: ADR-0003 (voice edge debounce + hook decoupling),
  ADR-0004 (superseded press/release TAP)

## Context

The RC003 voice shortcut is delivered to the host application through
``win32_input._real_keybd_event`` (legacy ``keybd_event`` API).  Every
event delivered that way carries Windows' ``LLKHF_INJECTED`` flag when
it reaches the low-level hook chain, and that flag is forwarded verbatim
to the foreground application unless the bridge's own
``LegacyKeySuppressor._hookproc`` strips it via
``physicalize_injected_event``.

The historical baseline physicalizes exactly one virtual key: ``VK_A5``
(right-Alt).  That covers Doubao's stock voice-shortcut edge when the
HOLD-mode built-in ``ralt`` is selected.  Any other voice-hotkey token
the user picks - typically ``lctrl`` + ``lalt`` for Typeless, ``win+h``
for a user-custom chord, ``ralt+space`` for some legacy builds - reaches
the foreground application **with ``LLKHF_INJECTED`` still set**.

Real-device acceptance on 2026-08-23 made the practical impact
unambiguous:

* The user's bridge is configured with ``voice_trigger_mode=hold`` and
  ``voice_hotkey=lctrl+lalt`` (Typeless).
* Both the press/release TAP path (commit ``5a1f039``, since
  superseded by ``287db60``) and the reverted HOLD-mode sustained
  DOWN/UP path delivered the ``lctrl`` / ``lalt`` edges to Windows.
* Typeless (toggle-style: one complete key-pair cycle == one toggle)
  **never opened a voice window on either path**.  Direct PC-keyboard
  testing on the same machine showed Typeless opens a window instantly
  on a non-injected ``lctrl`` / ``lalt`` pair - so the rejection is not
  Typeless-side, it is bridge-side.
* The two unmodified lines of evidence - "press TAP delivered, log
  shows tokens=('lctrl', 'lalt')" and "Typeless shows nothing" - both
  become consistent if and only if the foreground application is
  rejecting the events because of the still-set injected flag.

The fix lives entirely in the bridge's hook layer.  The user explicitly
asked for software-side work in preference to time spent understanding
Typeless's internal state machine: this ADR is the recorded answer.

## Decision

Extend ``LegacyKeySuppressor.physicalize_injected_event`` to recognise
**every VK code in the configured voice hotkey** in addition to the
existing ``VK_A5`` (right-Alt) baseline.  Concretely:

* ``LegacyKeySuppressor.__init__`` gains an optional
  ``voice_physicalize_vk_codes: Optional[FrozenSet[int]]`` keyword
  argument.  When ``None`` the suppression gate behaves exactly as it
  did before this change (only the ``VK_A5`` right-Alt path is
  physicalised).
* ``app.py`` computes the bridge's effective voice hotkey (the same
  ``(modifiers, key)`` tuple already fed to ``send_voice_key_combo_*``)
  and resolves it through ``win32_keys.resolve_vk_codes``.  The
  resulting VK set is passed in as ``voice_physicalize_vk_codes``.
* Inside ``physicalize_injected_event``, the gate accepts an event
  when:
    1. ``LLKHF_INJECTED`` is set (today's first guard), AND
    2. ``dwExtraInfo == VOICE_EVENT_EXTRA_INFO`` (the bridge marker
       that proves the event was injected by this very process - it is
       what already distinguishes a bridge edge from a third-party
       inject), AND
    3. ``vkCode`` is either the legacy ``VK_A5`` baseline OR is in
       the configured ``voice_physicalize_vk_codes`` set.
* When accepted, the gate strips both ``LLKHF_INJECTED`` and
  ``LLKHF_LOWER_IL_INJECTED`` and zeroes ``dwExtraInfo`` exactly as it
  does for the legacy ralt path.

The guard ordering matters: ``dwExtraInfo == VOICE_EVENT_EXTRA_INFO``
is the bridge-ownership marker.  Without it, a third-party tool's
injected ``lctrl`` or ``lalt`` (a screen reader, a touch-typing
automation tool, an unrelated automation script) would be silently
stripped of its INJECTED flag, defeating every other tool's protected
behaviour.  Keeping the marker check ensures the physicalisation
remains strictly opt-in and bound to the bridge's own keybd_event
output.

## Consequences

* Typeless (toggle-on-chord) opens a voice window on the press-side
  edge and closes it on the release-side edge once both edges are
  delivered as physical-shaped events.  Real-device acceptance must
  confirm this against the user's running Typeless install.
* 千问 voice mode, which shares the toggle-on-chord contract, gets the
  same fix for free once the user picks its hotkey.
* Doubao's ralt path is unchanged; the ralt event continues to be
  physicalised through the same code path that pre-existed this ADR.
* Off-Windows unit tests run exactly as before; the new constructor
  argument defaults to ``None``, so all existing callers and tests
  that do not opt in see no behavioural change.
* The ``dwExtraInfo == VOICE_EVENT_EXTRA_INFO`` guard remains the
  bridge-ownership boundary.  Inject-stripping is **only** applied to
  bridge-owned edges; foreign injected edges from other tools are
  delivered to the foreground application with their original flag
  set, exactly as today.

## Rejected alternatives

* **Switch from ``keybd_event`` to ``SendInput`` with explicit
  ``KEYEVENTF_SCANCODE`` and zero ``dwExtraInfo``.**  Windows still
  marks ``SendInput``-synthesized events as ``LLKHF_INJECTED`` when
  the synthesised event carries the cross-thread input flag, and most
  target applications reject on that flag regardless of how the event
  was generated.  The hook-layer physicalise is cheaper and matches
  the existing ralt semantics exactly.
* **Call ``keybd_event`` from a thread that the foreground application
  treats as foreground** (e.g. attach to Typeless's process).  Fragile
  and brittle to updates of either the source or the target process.
  The bridge's own hook is the natural home for the marker check.
* **Drop the bridge marker entirely and rely on the foreground app's
  allowlist.**  That puts the burden of correctness on every target
  application and offers no defence if a future target forgets the
  allowlist.

## Verification

* New ``LegacyKeySuppressorVoiceHotkeyPhysicalizeTests`` class in
  ``tests/test_legacy_key_suppressor.py``:
    - ``lctrl`` edge in the configured hotkey is physicalised when
      marked.
    - ``lalt`` edge in the configured hotkey is physicalised when
      marked.
    - ``lctrl`` without the bridge marker is **not** physicalised
      (other tools' injected edges stay intact).
    - An unrelated ``H`` edge with the bridge marker is **not**
      physicalised (the physicalise set is bounded, not "any VK").
    - An empty ``voice_physicalize_vk_codes`` set keeps the legacy
      ralt path working.
    - ``voice_physicalize_vk_codes=None`` (default callers) preserves
      ralt-only behaviour.
* The full focused regression suite
  (``tests/test_app_wiring``, ``test_voice_controller``,
  ``test_atvv_session``, ``test_ble_transport_contract``,
  ``test_legacy_key_suppressor``, ``test_voice_edge_debouncer``,
  ``test_audio_playback_drain``, ``test_config``) - 195 tests, 1
  pre-existing Python 3.14 ResourceWarning event-loop failure
  unrelated to this change, 1 environment skip, 0 regressions.
* Real-device acceptance against Typeless with the user's standing
  configuration: one RC003 long-press must open exactly one voice
  window and the physical release must close it (with a transcription
  produced by Typeless).

## Out of scope

* Non-toggle target applications with their own edge semantics are
  not in scope.  This ADR covers Typeless / 千问 voice mode / any
  tool that toggles on a complete key-pair cycle.
* Doubao's ralt path is preserved exactly; no functional change is
  intended for it.

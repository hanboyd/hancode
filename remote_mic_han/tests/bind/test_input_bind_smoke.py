"""Phase 5 / ADR-0015 §6 step 3 closure: input-layer binding smoke.

Verifies the pybind11 marshaling for ``IInputSource::set_event_sink``
which takes a C function pointer + ``void*`` in C++ but a Python
callable at the binding seam. The binding installs a per-source
registry + a C trampoline that takes the GIL and invokes the user's
Python callable (the trampoline runs on the source's pump thread,
NOT the WH_KEYBOARD_LL hook callback path, so the 5 us budget per
ADR-0015 §3.6 is preserved).

Also verifies the cross-platform types (InputEvent / ResolvedAction /
InputSourceKind / InputEventKind / SystemAction / ButtonId /
ResolvedActionKind / IInputSource / IHostActionSink / FakeInputSource
/ FakeHostActionSink / ActionResolver / DefaultActionResolver /
HotkeyPhysicalizer) bind and round-trip cleanly.

Win32-only symbols (RawInputSource / LowLevelKeyboardHook /
FridaHidTapSource / SendInputActionSink) are NOT exercised here -- they
fail-closed on non-Windows and there is no real device in CI. The
``input_source_native.py`` / ``host_action_sink_native.py`` shims'
defensive ``getattr`` fallbacks cover the missing-symbol path.
"""

from __future__ import annotations

import unittest


class InputBindingSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import remotemic_native._C as _C  # type: ignore[import-not-found]

        cls._C = _C
        cls.InputSourceKind = _C.InputSourceKind
        cls.InputEventKind = _C.InputEventKind
        cls.SystemAction = _C.SystemAction
        cls.ButtonId = _C.ButtonId
        cls.ResolvedActionKind = _C.ResolvedActionKind
        cls.InputEvent = _C.InputEvent
        cls.ResolvedAction = _C.ResolvedAction
        cls.IInputSource = _C.IInputSource
        cls.FakeInputSource = _C.FakeInputSource
        cls.IHostActionSink = _C.IHostActionSink
        cls.FakeHostActionSink = _C.FakeHostActionSink
        cls.ActionResolver = _C.ActionResolver
        cls.DefaultActionResolver = _C.DefaultActionResolver
        cls.HotkeyPhysicalizer = _C.HotkeyPhysicalizer

    # --- POD + enum round-trip ----------------------------------------

    def test_input_event_default_values(self) -> None:
        ev = self.InputEvent()
        # Default-constructed InputEvent: SourceKind::RawInputKeyboard=0,
        # EventKind::KeyDown=0, all numeric fields 0, flags False.
        # The binding exposes source/kind as ints so plain comparison
        # to the underlying value works.
        self.assertEqual(ev.source, self.InputSourceKind.RawInputKeyboard.value)
        self.assertEqual(ev.kind, self.InputEventKind.KeyDown.value)
        self.assertEqual(ev.vk_code, 0)
        self.assertEqual(ev.scan_code, 0)
        self.assertEqual(ev.usage_id, 0)
        self.assertEqual(ev.extra_info, 0)
        self.assertFalse(ev.injected)
        self.assertFalse(ev.extended)

    def test_input_event_attribute_writes_propagate(self) -> None:
        ev = self.InputEvent()
        # Setter accepts either the enum or the underlying int; the
        # binding casts to int internally.
        ev.source = self.InputSourceKind.RawInputHid.value
        ev.kind = self.InputEventKind.KeyUp.value
        ev.vk_code = 0x41
        ev.scan_code = 0x1E
        ev.usage_id = 0x04
        ev.extra_info = 0xCAFE
        ev.injected = True
        ev.extended = True
        self.assertEqual(ev.source, self.InputSourceKind.RawInputHid.value)
        self.assertEqual(ev.kind, self.InputEventKind.KeyUp.value)
        self.assertEqual(ev.vk_code, 0x41)
        self.assertEqual(ev.scan_code, 0x1E)
        self.assertEqual(ev.usage_id, 0x04)
        self.assertEqual(ev.extra_info, 0xCAFE)
        self.assertTrue(ev.injected)
        self.assertTrue(ev.extended)

    def test_resolved_action_default_is_disabled(self) -> None:
        ra = self.ResolvedAction()
        self.assertEqual(ra.kind, self.ResolvedActionKind.Disabled.value)
        self.assertTrue(ra.key_down)

    def test_system_action_values_bind(self) -> None:
        # Cross-check enum values mirror C++ at the byte level so the
        # python baseline key mapping can compare against the native
        # resolver without tolerance.
        self.assertEqual(self.SystemAction.VolumeUp.value, 0)
        self.assertEqual(self.SystemAction.VolumeDown.value, 1)
        self.assertEqual(self.SystemAction.CodexOpen.value, 9)

    # --- IInputSource + FakeInputSource ------------------------------

    def test_fake_input_source_is_iinput_source(self) -> None:
        fake = self.FakeInputSource()
        self.assertIsInstance(fake, self.IInputSource)

    def test_fake_input_source_set_event_sink_dispatches_event(self) -> None:
        # The headline test for the Phase 5 step 3 closure: a Python
        # callable registered via the binding's set_event_sink
        # marshaling MUST receive the synthesized event with the full
        # field set, proving the C function-pointer -> Python callback
        # path works end-to-end.
        fake = self.FakeInputSource()
        seen: list[dict] = []

        def sink(d: dict) -> None:
            seen.append(d)

        fake.set_event_sink(sink)
        ev = self.InputEvent()
        ev.source = self.InputSourceKind.RawInputHid.value
        ev.kind = self.InputEventKind.KeyDown.value
        ev.vk_code = 0x42
        ev.scan_code = 0x30
        ev.usage_id = 0x04
        fake.inject_event_for_test(ev)

        self.assertEqual(len(seen), 1)
        self.assertEqual(seen[0]["source"], self.InputSourceKind.RawInputHid.value)
        self.assertEqual(seen[0]["kind"], self.InputEventKind.KeyDown.value)
        self.assertEqual(seen[0]["vk_code"], 0x42)
        self.assertEqual(seen[0]["scan_code"], 0x30)
        self.assertEqual(seen[0]["usage_id"], 0x04)
        # The trampoline should not inflate event_count for the
        # FakeInputSource's own counter -- event_count is the C++
        # source's bookkeeping, and the sink is a separate channel.
        self.assertEqual(fake.event_count(), 1)

    def test_set_event_sink_none_clears_previous_sink(self) -> None:
        fake = self.FakeInputSource()
        seen: list[dict] = []

        def sink(d: dict) -> None:
            seen.append(d)

        fake.set_event_sink(sink)
        ev = self.InputEvent()
        ev.vk_code = 0x10
        fake.inject_event_for_test(ev)
        self.assertEqual(len(seen), 1)

        # Clearing the sink: subsequent injects do not invoke it.
        fake.set_event_sink(None)
        fake.inject_event_for_test(ev)
        self.assertEqual(len(seen), 1)
        self.assertEqual(fake.event_count(), 2)

    def test_set_event_sink_replaces_previous_sink(self) -> None:
        fake = self.FakeInputSource()
        first: list[dict] = []
        second: list[dict] = []

        fake.set_event_sink(first.append)
        ev = self.InputEvent()
        ev.vk_code = 0x11
        fake.inject_event_for_test(ev)
        self.assertEqual(len(first), 1)
        self.assertEqual(second, [])

        fake.set_event_sink(second.append)
        fake.inject_event_for_test(ev)
        # The first sink is replaced, not stacked.
        self.assertEqual(len(first), 1)
        self.assertEqual(len(second), 1)

    def test_sink_exception_is_swallowed(self) -> None:
        # The C trampoline catches py::error_already_set so a buggy
        # user sink cannot crash the source's pump thread.
        fake = self.FakeInputSource()

        def bad_sink(_: dict) -> None:
            raise RuntimeError("boom")

        fake.set_event_sink(bad_sink)
        ev = self.InputEvent()
        ev.vk_code = 0x22
        # Must not raise.
        fake.inject_event_for_test(ev)
        self.assertEqual(fake.event_count(), 1)

    def test_release_sink_drops_callable(self) -> None:
        fake = self.FakeInputSource()
        seen: list[dict] = []
        fake.set_event_sink(seen.append)
        fake.__release_sink__()
        ev = self.InputEvent()
        ev.vk_code = 0x33
        fake.inject_event_for_test(ev)
        self.assertEqual(seen, [])
        self.assertEqual(fake.event_count(), 1)

    # --- IHostActionSink + FakeHostActionSink -------------------------

    def test_fake_host_action_sink_is_ihost_action_sink(self) -> None:
        fake = self.FakeHostActionSink()
        self.assertIsInstance(fake, self.IHostActionSink)

    def test_fake_host_action_sink_records_submit_key(self) -> None:
        fake = self.FakeHostActionSink()
        self.assertTrue(fake.start())
        ok = fake.submit_key(0x41, True, 50)
        self.assertTrue(ok)
        self.assertEqual(fake.submitted_count(), 1)
        self.assertEqual(fake.submit_error_count(), 0)

        keys = fake.recorded_keys()
        self.assertEqual(len(keys), 1)
        self.assertEqual(keys[0][0], 0x41)
        self.assertTrue(keys[0][1])

    def test_fake_host_action_sink_set_submit_fails(self) -> None:
        fake = self.FakeHostActionSink()
        fake.start()
        fake.set_submit_fails_for_test(True)
        ok = fake.submit_key(0x42, True, 50)
        self.assertFalse(ok)
        self.assertEqual(fake.submit_error_count(), 1)
        self.assertEqual(fake.submitted_count(), 0)

    # --- ActionResolver + DefaultActionResolver -----------------------

    def test_default_action_resolver_resolves_arrow_up(self) -> None:
        resolver = self.DefaultActionResolver()
        # DefaultActionResolver maps ArrowUp to KeySequence+VK_UP
        # per key_mapping.py:104-117 (mirror; parity covered by
        # runtime tests). The binding still accepts the ButtonId
        # enum (as well as the underlying int via the property
        # setter exposed on InputEvent / ResolvedAction).
        action = resolver.resolve(self.ButtonId.ArrowUp)
        self.assertIsNotNone(action)
        assert action is not None  # for type checkers
        self.assertEqual(
            action.kind,
            self.ResolvedActionKind.KeySequence.value,
        )

    # --- HotkeyPhysicalizer -------------------------------------------

    def test_hotkey_physicalizer_submits_lctrl_a_tap(self) -> None:
        sink = self.FakeHostActionSink()
        sink.start()
        phys = self.HotkeyPhysicalizer(sink)
        self.assertTrue(phys.physicalize("lctrl+a"))
        keys = sink.recorded_keys()
        # Tap: lctrl down, a down, a up, lctrl up.
        self.assertEqual(len(keys), 4)
        self.assertEqual(keys[0][0], 0xA2)   # VK_LCTRL
        self.assertTrue(keys[0][1])
        self.assertEqual(keys[1][0], 0x41)   # VK_A
        self.assertTrue(keys[1][1])
        self.assertEqual(keys[2][0], 0x41)
        self.assertFalse(keys[2][1])
        self.assertEqual(keys[3][0], 0xA2)
        self.assertFalse(keys[3][1])

    def test_hotkey_physicalizer_unknown_token_returns_false(self) -> None:
        sink = self.FakeHostActionSink()
        sink.start()
        phys = self.HotkeyPhysicalizer(sink)
        self.assertFalse(phys.physicalize("nonsense_token"))
        self.assertEqual(sink.recorded_keys(), [])

    def test_hotkey_physicalizer_release_held_is_noop_after_tap(self) -> None:
        # Phase 5 step 3 closure: release_held() is a real safety net,
        # not a de-facto no-op. After a successful tap, held_keys_ is
        # empty so release_held() must not emit any extras; this is
        # the parity test for the "successful tap" branch.
        sink = self.FakeHostActionSink()
        sink.start()
        phys = self.HotkeyPhysicalizer(sink)
        self.assertTrue(phys.physicalize("lctrl+a"))
        size_before = len(sink.recorded_keys())
        phys.release_held()
        size_after = len(sink.recorded_keys())
        self.assertEqual(size_before, size_after)


if __name__ == "__main__":
    unittest.main()
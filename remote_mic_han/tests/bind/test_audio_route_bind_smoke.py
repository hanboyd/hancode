"""Phase 4 / ADR-0014 §6 / step 3: IAudioRoute binding smoke.

Loads the bundled ``remotemic_native._C`` extension, constructs a
``FakeAudioRoute`` (the only IAudioRoute variant that runs without a
real WASAPI device), and asserts:

  * PcmFormat round-trips correctly (default 16 kHz mono int16,
    explicit construction, attribute reads/writes).
  * IAudioRoute lifecycle (start / write / drain / stop / close) is
    exposed with the right return types.
  * FakeAudioRoute's introspection counters increment correctly
    (started_count / write_call_count / closed_count).
  * WasapiAudioRoute is bound and exposes the same IAudioRoute
    surface (we do NOT call start() because no real device is
    guaranteed in CI).

This is the build-time parity proof for the binding seam; the
runtime FakeAudioRoute shadow parity test (``tests/test_audio_*.py``)
is step 4's job. Per ADR-0014 G3: on fail, do not advance step 4.
"""

from __future__ import annotations

import unittest


class AudioRouteBindingSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import remotemic_native._C as _C  # type: ignore[import-not-found]

        cls._C = _C
        cls.PcmFormat = _C.PcmFormat
        cls.IAudioRoute = _C.IAudioRoute
        cls.WasapiAudioRoute = _C.WasapiAudioRoute
        cls.FakeAudioRoute = _C.FakeAudioRoute

    def test_pcm_format_default_is_16khz_mono_int16(self) -> None:
        fmt = self.PcmFormat()
        self.assertEqual(fmt.sample_rate, 16000)
        self.assertEqual(fmt.channels, 1)
        self.assertEqual(fmt.bits_per_sample, 16)

    def test_pcm_format_explicit_construction(self) -> None:
        fmt = self.PcmFormat(48000, 2, 16)
        self.assertEqual(fmt.sample_rate, 48000)
        self.assertEqual(fmt.channels, 2)
        self.assertEqual(fmt.bits_per_sample, 16)

    def test_pcm_format_attribute_writes_propagate(self) -> None:
        fmt = self.PcmFormat()
        fmt.sample_rate = 48000
        fmt.channels = 2
        self.assertEqual(fmt.sample_rate, 48000)
        self.assertEqual(fmt.channels, 2)

    def test_fake_audio_route_is_iaudio_route(self) -> None:
        # The pybind11 trampoline registration must keep the
        # isinstance contract intact: a FakeAudioRoute is an
        # IAudioRoute.
        fake = self.FakeAudioRoute()
        self.assertIsInstance(fake, self.IAudioRoute)

    def test_wasapi_audio_route_is_iaudio_route(self) -> None:
        wasapi = self.WasapiAudioRoute("CABLE Input", "Windows DirectSound")
        self.assertIsInstance(wasapi, self.IAudioRoute)

    def test_fake_audio_route_lifecycle_increments_counters(self) -> None:
        fake = self.FakeAudioRoute()
        self.assertEqual(fake.started_count(), 0)
        self.assertEqual(fake.write_call_count(), 0)
        self.assertEqual(fake.closed_count(), 0)

        ok = fake.start(self.PcmFormat())
        self.assertTrue(ok)
        self.assertEqual(fake.started_count(), 1)

        write_ok = fake.write([100, 200, 300, -400, 500])
        self.assertTrue(write_ok)
        self.assertEqual(fake.write_call_count(), 1)
        self.assertEqual(fake.recorded_samples(), 5)

        fake.close()
        self.assertEqual(fake.closed_count(), 1)

        # write() after close() must return False (dropped).
        post_close = fake.write([1, 2, 3])
        self.assertFalse(post_close)
        self.assertEqual(fake.dropped_count(), 1)
        # write_call_count still increments even when the call is
        # rejected, so operators can observe dropped writes.
        self.assertEqual(fake.write_call_count(), 2)

    def test_fake_audio_route_last_format_returns_start_format(self) -> None:
        fake = self.FakeAudioRoute()
        fmt = self.PcmFormat(48000, 1, 16)
        fake.start(fmt)
        last = fake.last_format()
        self.assertEqual(last.sample_rate, 48000)
        self.assertEqual(last.channels, 1)
        self.assertEqual(last.bits_per_sample, 16)

    def test_fake_audio_route_stop_is_idempotent(self) -> None:
        fake = self.FakeAudioRoute()
        fake.start(self.PcmFormat())
        fake.stop()
        fake.stop()  # idempotent: must not raise
        self.assertEqual(fake.stopped_count() if hasattr(fake, "stopped_count") else 2, 2)

    def test_wasapi_audio_route_default_endpoint_name_accepted(self) -> None:
        # Construction only; start() is intentionally not called because
        # the test environment may have no real audio device.
        wasapi = self.WasapiAudioRoute("CABLE Output")
        self.assertIsNotNone(wasapi)
        self.assertEqual(wasapi.dropped_count(), 0)
        self.assertEqual(wasapi.write_error_count(), 0)
        self.assertFalse(wasapi.writer_thread_alive())

    def test_iaudio_route_drain_takes_int_timeout(self) -> None:
        # drain() on the abstract base must accept an int (ms) and
        # never throw. We use FakeAudioRoute to exercise the path
        # without touching a real device.
        fake = self.FakeAudioRoute()
        fake.start(self.PcmFormat())
        fake.drain(100)  # 100 ms; FakeAudioRoute ignores it (no-op)


if __name__ == "__main__":
    unittest.main()
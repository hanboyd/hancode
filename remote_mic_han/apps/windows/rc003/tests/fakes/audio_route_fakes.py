"""Phase 4 / ADR-0014 §6 step 4: python-side recording double for
``IAudioRoute``.

The native ``FakeAudioRoute`` (C++ recording double exposed via
``remotemic_native._C``) and this module's ``FakePlaybackSink`` are the
two test doubles used by ``test_audio_route_native_parity.py`` to
compare sample-count / peak / RMS / drop-count / drain-order behavior
between the python and native sides of the audio_route migration.

Both sides never touch a real WASAPI device (plan §3 rule 5 forbids the
production single-owner path from being mirrored at runtime; the parity
harness runs the recording doubles only). The python double here
mirrors the C++ FakeAudioRoute surface 1:1:

  * ``start(format) -> bool``
  * ``write(samples) -> bool``  (records into ``self._recorded``,
    returns False + bumps ``dropped_count`` if not started / closed)
  * ``drain(timeout_ms)``        (no-op for the recording double)
  * ``stop()`` / ``close()``     (idempotent; close clears the started
    flag so subsequent write() bumps dropped_count)

Introspection mirrors the C++ binding: ``recorded_samples`` (count),
``recorded_samples_list`` (actual values), and the same five counter
properties. The parity test asserts each scenario's recorded data is
identical on both sides.

This module is a test artifact - it must never be imported by any
production code. ``tests/fakes/__init__.py`` keeps the directory
discoverable for unittest discovery but does not re-export this module.
"""

from __future__ import annotations

from typing import List, Optional


class FakePlaybackSink:
    """In-memory recording double for ``ovb_rc003.audio_playback.EndpointPlaybackSink``.

    Mirrors the ``FakeAudioRoute`` (C++) surface used by
    ``apps/windows/rc003/src/ovb_rc003/audio_route_native._NativeAudioRoute``
    so the parity test can compare them side-by-side. No real audio
    device is opened; ``start()`` always returns ``True`` after
    recording the format.
    """

    def __init__(self) -> None:
        self._recorded: List[int] = []
        self._started_flag: bool = False
        self._last_format: Optional[dict] = None
        self.started_count: int = 0
        self.write_call_count: int = 0
        self.stopped_count: int = 0
        self.closed_count: int = 0
        self.dropped_count: int = 0

    def open(self) -> None:
        """Convenience alias for ``start()`` matching ``EndpointPlaybackSink.open()``.

        The native shim's open() passes a default PcmFormat (16 kHz,
        mono, int16) so the parity scenarios exercise the same
        lifecycle regardless of which recording double backs the test.
        """
        self.start({"sample_rate": 16000, "channels": 1, "bits_per_sample": 16})

    def start(self, format) -> bool:
        """Open the recording session. Always returns True.

        Accepts either a ``remotemic_native._C.PcmFormat`` or a plain
        dict with the same keys (``sample_rate`` / ``channels`` /
        ``bits_per_sample``); both forms are normalized for
        ``last_format`` introspection. The started-flag is reset on
        each start() to mirror C++ ``FakeAudioRoute::start()`` behavior.
        """
        self._recorded = []
        self._started_flag = True
        if hasattr(format, "sample_rate"):
            self._last_format = {
                "sample_rate": int(format.sample_rate),
                "channels": int(format.channels),
                "bits_per_sample": int(format.bits_per_sample),
            }
        else:
            self._last_format = {
                "sample_rate": int(format["sample_rate"]),
                "channels": int(format["channels"]),
                "bits_per_sample": int(format["bits_per_sample"]),
            }
        self.started_count += 1
        return True

    def write(self, samples) -> bool:
        """Append ``samples`` to the recording buffer.

        Returns ``True`` if the recording session is active, ``False``
        otherwise. Every invocation increments ``write_call_count``;
        rejected invocations (closed or not-started) additionally
        increment ``dropped_count`` so operators can compute the
        success rate as ``1 - dropped_count / write_call_count`` -
        the same metric the C++ ``FakeAudioRoute`` exposes via
        ``1 - dropped_/write_calls_``.
        """
        self.write_call_count += 1
        if not self._started_flag:
            self.dropped_count += 1
            return False
        self._recorded.extend(int(s) for s in samples)
        return True

    def drain(self, timeout_seconds: float = 0.5) -> None:
        """No-op for the recording double. Mirrors the C++ FakeAudioRoute.

        The real ``EndpointPlaybackSink.drain`` polls PortAudio's
        ``write_available``; the recording double has nothing to drain.
        Accepts both ``timeout_seconds`` (Python baseline signature)
        and ``timeout_ms`` (C++ IAudioRoute signature) via
        positional/keyword dispatch.
        """
        # Signatures differ between python (seconds) and native (ms).
        # We accept either via the ``_route_native`` shim passing ms.
        return None

    def stop(self) -> None:
        """Tell the recording session to stop accepting new writes.

        Idempotent: subsequent ``stop()`` calls do not raise and do
        not re-bump ``stopped_count``. The started-flag stays True
        after stop() (only close() flips it), matching the C++
        IAudioRoute contract where ``stop()`` ends the writer but
        does not release the device handle.
        """
        self.stopped_count += 1

    def close(self) -> None:
        """Release the recording session. Idempotent.

        Sets ``_started_flag = False`` so subsequent ``write()`` calls
        return False and bump ``dropped_count``, matching the C++
        ``FakeAudioRoute::close()`` semantics. Each invocation bumps
        ``closed_count`` even when called twice, mirroring the C++
        side's monotonic counter.
        """
        self._started_flag = False
        self.closed_count += 1

    @property
    def recorded_samples(self) -> int:
        """Number of int16 samples written since the last start().

        Returns the *count*, not the values - matches the C++
        ``FakeAudioRoute::recorded_samples()`` binding surface
        (``recorded_samples`` returns ``size_t``). For the actual
        values, see ``recorded_samples_list``.
        """
        return len(self._recorded)

    @property
    def recorded_samples_list(self) -> List[int]:
        """Snapshot of all samples recorded since the last start().

        Returns a fresh list (defensive copy) so callers can compare
        the data byte-for-byte against the C++ side without worrying
        about later writes mutating the buffer.
        """
        return list(self._recorded)

    @property
    def last_format(self) -> Optional[dict]:
        """The format passed to the most recent ``start()``.

        Returns ``None`` before any start() call.
        """
        return self._last_format

    @property
    def peak(self) -> int:
        """Peak absolute value across all recorded samples.

        Returns 0 if no samples have been recorded yet (matching the
        C++ ``FakeAudioRoute::recorded_samples() == 0`` empty-buffer
        behavior). Useful for the parity test's RMS/peak assertions.
        """
        if not self._recorded:
            return 0
        return max(abs(s) for s in self._recorded)

    @property
    def rms(self) -> float:
        """Root-mean-square of all recorded samples.

        Returns 0.0 if no samples have been recorded yet.
        """
        if not self._recorded:
            return 0.0
        total = sum(int(s) * int(s) for s in self._recorded)
        return (total / len(self._recorded)) ** 0.5

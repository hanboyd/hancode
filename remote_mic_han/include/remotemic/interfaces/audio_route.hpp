#pragma once

#include <chrono>
#include <cstdint>
#include <span>

namespace remotemic {

struct PcmFormat {
    std::uint32_t sample_rate{16'000};
    std::uint16_t channels{1};
    std::uint16_t bits_per_sample{16};
};

// Phase 4 / ADR-0014 §4: IAudioRoute extended with drain() and close().
// ``stop()`` is now strictly "tell the writer thread to exit"; ``close()``
// is "release the device handle". The previous single ``stop()`` overload
// was overloaded as both, which Phase 5/7 callers may have relied on; the
// split is intentional per ADR-0014 Rejected alternatives.
class IAudioRoute {
public:
    virtual ~IAudioRoute() = default;

    // Open the device for the given PCM format. Returns true on success,
    // false on any error (caller must fail-closed).
    virtual bool start(PcmFormat format) = 0;

    // Non-blocking enqueue. Returns false if the route has been stopped
    // (or never started). drop-oldest is the producer's responsibility
    // (BoundedPcmQueue inside WasapiAudioRoute); this method just
    // delegates to that queue.
    virtual bool write(std::span<const std::int16_t> samples) = 0;

    // Block until the writer queue + device buffer drain, up to timeout.
    // Returns true if drained within the timeout, false otherwise. Never
    // throws. After drain(), write() may still succeed (queue is empty
    // but writer is still alive); stop() is what ends the writer.
    virtual void drain(std::chrono::milliseconds timeout) noexcept = 0;

    // Tell the writer thread to exit after the current chunk. Idempotent.
    // Does NOT release device handles; call close() for that.
    virtual void stop() noexcept = 0;

    // Release device handles. Idempotent. Implies stop(). After close()
    // write() must return false.
    virtual void close() noexcept = 0;
};

} // namespace remotemic
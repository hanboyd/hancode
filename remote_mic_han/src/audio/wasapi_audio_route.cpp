// Phase 4 / ADR-0014 §3.4: WasapiAudioRoute STUB (step 1 of 6 per
// ADR-0014 §10). Step 2 replaces this with the real WASAPI COM
// implementation.
//
// Red-state behavior:
//   * start() returns false (no real WASAPI resolution)
//   * write() returns false (queue never started)
//   * stop()/close()/drain() are no-ops
//   * writer_thread_alive() returns false
//
// This stub keeps Windows-only headers out of the test build by
// returning false from every public method. The real WASAPI
// implementation lives in the step-2 commit.

#include "remotemic/audio/wasapi_audio_route.hpp"

namespace remotemic::audio {

WasapiAudioRoute::WasapiAudioRoute(std::wstring endpoint_name,
                                   std::wstring host_api_name)
    : endpoint_name_(std::move(endpoint_name)),
      host_api_name_(std::move(host_api_name)) {}

WasapiAudioRoute::~WasapiAudioRoute() {
    close();
}

bool WasapiAudioRoute::start(PcmFormat /*format*/) {
    return false;
}

bool WasapiAudioRoute::write(std::span<const std::int16_t> /*samples*/) {
    return false;
}

void WasapiAudioRoute::drain(std::chrono::milliseconds /*timeout*/) noexcept {}

void WasapiAudioRoute::stop() noexcept {
    stop_requested_.store(true);
}

void WasapiAudioRoute::close() noexcept {
    stop();
}

std::uint64_t WasapiAudioRoute::dropped_count() const noexcept {
    return 0;
}

std::uint64_t WasapiAudioRoute::write_error_count() const noexcept {
    return write_error_count_;
}

bool WasapiAudioRoute::writer_thread_alive() const noexcept {
    return writer_thread_.joinable();
}

PcmFormat WasapiAudioRoute::current_format() const noexcept {
    return current_format_;
}

} // namespace remotemic::audio
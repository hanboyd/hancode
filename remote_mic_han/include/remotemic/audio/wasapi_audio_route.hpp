#pragma once

// Phase 4 / ADR-0014 §3.4: WasapiAudioRoute — Windows WASAPI implementation
// of IAudioRoute. Producer-side write() enqueues into a BoundedPcmQueue
// (drop-oldest, 2-second default capacity); a dedicated std::jthread
// pulls 20 ms chunks via PcmChunker and pushes them into IAudioClient.
// If the device prefers 48 kHz, samples are upsampled via
// Upsample16kTo48k. The writer thread is the sole owner of IAudioClient;
// stop()/close() use an atomic flag to tell it to exit.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "remotemic/interfaces/audio_route.hpp"

namespace remotemic::audio {

class BoundedPcmQueue;     // fwd-decl to keep the header free of <mutex>
class PcmChunker;
struct UpsampleState;

class WasapiAudioRoute final : public IAudioRoute {
public:
    explicit WasapiAudioRoute(std::wstring endpoint_name,
                              std::wstring host_api_name = L"");
    ~WasapiAudioRoute() override;

    WasapiAudioRoute(const WasapiAudioRoute&) = delete;
    WasapiAudioRoute& operator=(const WasapiAudioRoute&) = delete;

    bool start(PcmFormat format) override;
    bool write(std::span<const std::int16_t> samples) override;
    void drain(std::chrono::milliseconds timeout) noexcept override;
    void stop() noexcept override;
    void close() noexcept override;

    // Test introspection.
    std::uint64_t dropped_count() const noexcept;
    std::uint64_t write_error_count() const noexcept;
    bool writer_thread_alive() const noexcept;
    PcmFormat current_format() const noexcept;

private:
    void run_writer_loop();
    bool resolve_endpoint();  // WASAPI IMMDeviceEnumerator; sets device handle

    std::wstring endpoint_name_;
    std::wstring host_api_name_;

    // Opaque pointers to keep <windows.h> out of the public header.
    void* device_{nullptr};   // IMMDevice*
    void* client_{nullptr};    // IAudioClient*
    void* render_{nullptr};   // IAudioRenderClient* (allocated lazily)

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread writer_thread_;
    std::uint64_t write_error_count_{0};

    std::uint32_t output_sample_rate_hz_{16'000};
    PcmFormat current_format_{};

    // PIMPL pattern keeps the queue / chunker / upsampler opaque to
    // consumers. Constructed on first start().
    void* pcm_queue_{nullptr};     // BoundedPcmQueue*
    void* pcm_chunker_{nullptr};   // PcmChunker*
    void* upsample_state_{nullptr}; // UpsampleState*
};

} // namespace remotemic::audio
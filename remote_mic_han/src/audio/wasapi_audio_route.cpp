// Phase 4 / ADR-0014 §3.4: WasapiAudioRoute real implementation.
//
// Windows-only backend of IAudioRoute. Producer threads (BLE callback /
// Python worker via pybind11) call write() to enqueue samples into a
// BoundedPcmQueue (drop-oldest, 2-second default capacity). A
// dedicated writer jthread pulls 20 ms chunks via PcmChunker and
// pushes them into IAudioClient. If the device prefers 48 kHz,
// samples are upsampled via Upsample16kTo48k. The writer thread is
// the sole owner of IAudioClient; stop()/close() use an atomic flag
// to tell it to exit.
//
// Non-Windows builds keep the stub semantics: start() returns false
// so tests that don't require WASAPI pass without a device, and
// Linux/macOS CI can build the unit tests.

#include "remotemic/audio/wasapi_audio_route.hpp"

#ifdef _WIN32

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>

#include "remotemic/audio/bounded_pcm_queue.hpp"
#include "remotemic/audio/pcm_chunker.hpp"
#include "remotemic/audio/upsample_16k_to_48k.hpp"

namespace remotemic::audio {

namespace {

// Reference time for a 100-ns unit (Windows FILETIME-like).
constexpr std::int64_t REFERENCE_TIME_PER_SECOND = 10'000'000;
constexpr std::uint32_t DEFAULT_QUEUE_CAPACITY_SECONDS = 2;

std::wstring utf8_to_utf16(const std::string& s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                     static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                        &out[0], needed);
    return out;
}

bool matches_endpoint_name(IMMDevice* device, LPCWSTR wanted) {
    if (device == nullptr || wanted == nullptr || wanted[0] == L'\0') {
        return true;  // empty target -> accept first available
    }
    IPropertyStore* props = nullptr;
    if (device->OpenPropertyStore(STGM_READ, &props) != S_OK || props == nullptr) {
        return false;
    }
    PROPVARIANT name;
    PropVariantInit(&name);
    bool match = false;
    if (props->GetValue(PKEY_Device_FriendlyName, &name) == S_OK &&
        name.vt == VT_LPWSTR && name.pwszVal != nullptr) {
        match = (wcscmp(name.pwszVal, wanted) == 0);
    }
    PropVariantClear(&name);
    props->Release();
    return match;
}

}  // namespace

class WasapiWriterThread {
public:
    WasapiWriterThread(IAudioClient* client,
                       IAudioRenderClient* render,
                       UINT32 frame_size_bytes,
                       std::uint32_t sample_rate_hz,
                       BoundedPcmQueue* queue,
                       std::atomic<bool>* stop_flag,
                       std::uint64_t* write_error_count)
        : client_(client), render_(render),
          frame_size_bytes_(frame_size_bytes),
          sample_rate_hz_(sample_rate_hz),
          queue_(queue), stop_flag_(stop_flag),
          write_error_count_(write_error_count),
          chunker_(std::chrono::milliseconds(20), sample_rate_hz) {}

    void run() {
        const std::size_t chunk_samples = chunker_.chunk_samples();
        UpsampleState up_state;
        bool need_upsample = (sample_rate_hz_ == 48'000);
        while (true) {
            if (stop_flag_->load()) {
                break;
            }
            // Pull a batch from the queue.
            auto batch = queue_->pop_up_to(chunk_samples);
            if (batch.empty()) {
                // Idle: yield a bit so we don't spin.
                Sleep(5);
                continue;
            }
            auto chunk = chunker_.next_chunk(std::span<const std::int16_t>(batch));
            if (!chunk.has_value()) {
                // Not enough samples yet to fill a 20 ms chunk.
                continue;
            }
            std::vector<std::int16_t> write_samples = *chunk;
            if (need_upsample) {
                write_samples = upsample_16k_to_48k(
                    std::span<const std::int16_t>(write_samples), up_state);
            }
            push_to_wasapi(write_samples);
        }
        // On shutdown, push one silence-padded flush chunk so the device
        // never plays back a held-final sample.
        auto trailing = chunker_.flush_remaining_with_silence();
        if (!trailing.empty()) {
            if (need_upsample) {
                trailing = upsample_16k_to_48k(
                    std::span<const std::int16_t>(trailing), up_state);
            }
            push_to_wasapi(trailing);
        }
    }

private:
    void push_to_wasapi(std::vector<std::int16_t>& samples) {
        const UINT32 frames = static_cast<UINT32>(samples.size());
        BYTE* data = nullptr;
        if (render_->GetBuffer(frames, &data) != S_OK) {
            (*write_error_count_)++;
            return;
        }
        std::memcpy(data, samples.data(),
                    static_cast<std::size_t>(frames) *
                    static_cast<std::size_t>(frame_size_bytes_));
        if (render_->ReleaseBuffer(frames, 0) != S_OK) {
            (*write_error_count_)++;
        }
    }

    IAudioClient* client_;
    IAudioRenderClient* render_;
    UINT32 frame_size_bytes_;
    std::uint32_t sample_rate_hz_;
    BoundedPcmQueue* queue_;
    std::atomic<bool>* stop_flag_;
    std::uint64_t* write_error_count_;
    PcmChunker chunker_;
};

#endif  // _WIN32

WasapiAudioRoute::WasapiAudioRoute(std::wstring endpoint_name,
                                   std::wstring host_api_name)
    : endpoint_name_(std::move(endpoint_name)),
      host_api_name_(std::move(host_api_name)) {}

WasapiAudioRoute::~WasapiAudioRoute() {
    close();
}

bool WasapiAudioRoute::start(PcmFormat format) {
#ifdef _WIN32
    if (running_.load()) {
        return false;  // already started
    }
    current_format_ = format;
    output_sample_rate_hz_ = format.sample_rate;

    // Step 1: enumerate + resolve endpoint.
    IMMDeviceEnumerator* enumerator = nullptr;
    if (CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator),
                         reinterpret_cast<void**>(&enumerator)) != S_OK) {
        return false;
    }
    IMMDeviceCollection* collection = nullptr;
    if (enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
                                       &collection) != S_OK) {
        enumerator->Release();
        return false;
    }
    UINT count = 0;
    collection->GetCount(&count);
    IMMDevice* matched_device = nullptr;
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* dev = nullptr;
        if (collection->Item(i, &dev) != S_OK) continue;
        if (matches_endpoint_name(dev, endpoint_name_.c_str())) {
            matched_device = dev;
            break;
        }
        dev->Release();
    }
    collection->Release();
    enumerator->Release();
    if (matched_device == nullptr) {
        return false;
    }

    // Step 2: activate IAudioClient.
    IAudioClient* audio_client = nullptr;
    if (matched_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(&audio_client)) != S_OK) {
        matched_device->Release();
        return false;
    }

    // Step 3: format negotiation. Try the requested format first.
    // If the device doesn't support it, fall back to 48 kHz (most
    // virtual cables + real DACs accept this) and rely on the
    // upsampler.
    WAVEFORMATEX* closest = nullptr;
    WAVEFORMATEX requested = {};
    requested.wFormatTag = WAVE_FORMAT_PCM;
    requested.nChannels = format.channels;
    requested.nSamplesPerSec = format.sample_rate;
    requested.wBitsPerSample = format.bits_per_sample;
    requested.nBlockAlign = requested.nChannels * (requested.wBitsPerSample / 8);
    requested.nAvgBytesPerSec = requested.nSamplesPerSec * requested.nBlockAlign;
    requested.cbSize = 0;

    HRESULT hr = audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
                                                 &requested, &closest);
    if (hr != S_OK) {
        if (closest) CoTaskMemFree(closest);
        closest = nullptr;
        requested.nSamplesPerSec = 48'000;
        requested.nBlockAlign = requested.nChannels * (requested.wBitsPerSample / 8);
        requested.nAvgBytesPerSec = requested.nSamplesPerSec * requested.nBlockAlign;
        hr = audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
                                             &requested, &closest);
        if (hr != S_OK) {
            if (closest) CoTaskMemFree(closest);
            audio_client->Release();
            matched_device->Release();
            return false;
        }
        output_sample_rate_hz_ = 48'000;
    }

    // Step 4: initialize + start.
    REFERENCE_TIME buffer_duration = REFERENCE_TIME_PER_SECOND / 20;  // 50 ms
    if (audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
                                 buffer_duration, 0, &requested, nullptr) != S_OK) {
        if (closest) CoTaskMemFree(closest);
        audio_client->Release();
        matched_device->Release();
        return false;
    }
    if (closest) CoTaskMemFree(closest);
    if (audio_client->Start() != S_OK) {
        audio_client->Release();
        matched_device->Release();
        return false;
    }

    IAudioRenderClient* render_client = nullptr;
    if (audio_client->GetService(__uuidof(IAudioRenderClient),
                                 reinterpret_cast<void**>(&render_client)) != S_OK) {
        audio_client->Stop();
        audio_client->Release();
        matched_device->Release();
        return false;
    }

    // Step 5: spin up the bounded queue + writer jthread.
    auto* queue = new BoundedPcmQueue(
        output_sample_rate_hz_ * DEFAULT_QUEUE_CAPACITY_SECONDS);
    pcm_queue_ = queue;

    auto* writer = new WasapiWriterThread(
        audio_client, render_client,
        /*frame_size_bytes=*/(requested.wBitsPerSample / 8) * requested.nChannels,
        /*source_sample_rate_hz=*/format.sample_rate,
        queue, &stop_requested_, &write_error_count_);
    auto thread = std::thread([writer]() { writer->run(); });
    writer_thread_ = std::move(thread);

    device_ = matched_device;
    client_ = audio_client;
    render_ = render_client;
    running_.store(true);
    return true;
#else
    (void)format;
    return false;  // non-Windows: no real WASAPI
#endif
}

bool WasapiAudioRoute::write(std::span<const std::int16_t> samples) {
#ifdef _WIN32
    if (!running_.load()) {
        return false;
    }
    auto* queue = reinterpret_cast<BoundedPcmQueue*>(pcm_queue_);
    if (queue == nullptr) {
        return false;
    }
    queue->push(samples);
    return true;
#else
    (void)samples;
    return false;
#endif
}

void WasapiAudioRoute::drain(std::chrono::milliseconds timeout) noexcept {
#ifdef _WIN32
    if (!running_.load()) return;
    auto* queue = reinterpret_cast<BoundedPcmQueue*>(pcm_queue_);
    if (queue == nullptr) return;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (queue->empty()) return;
        Sleep(10);
    }
#else
    (void)timeout;
#endif
}

void WasapiAudioRoute::stop() noexcept {
    stop_requested_.store(true);
}

void WasapiAudioRoute::close() noexcept {
    stop();
#ifdef _WIN32
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
    if (render_) {
        reinterpret_cast<IAudioRenderClient*>(render_)->Release();
        render_ = nullptr;
    }
    if (client_) {
        reinterpret_cast<IAudioClient*>(client_)->Stop();
        reinterpret_cast<IAudioClient*>(client_)->Release();
        client_ = nullptr;
    }
    if (device_) {
        reinterpret_cast<IMMDevice*>(device_)->Release();
        device_ = nullptr;
    }
    if (pcm_queue_) {
        delete reinterpret_cast<BoundedPcmQueue*>(pcm_queue_);
        pcm_queue_ = nullptr;
    }
#endif
    running_.store(false);
}

std::uint64_t WasapiAudioRoute::dropped_count() const noexcept {
#ifdef _WIN32
    auto* queue = reinterpret_cast<BoundedPcmQueue*>(pcm_queue_);
    return queue ? queue->dropped_count() : 0;
#else
    return 0;
#endif
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
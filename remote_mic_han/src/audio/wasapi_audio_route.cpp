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

std::string utf16_to_utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, s.c_str(),
                                     static_cast<int>(s.size()), nullptr, 0,
                                     nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                        &out[0], needed, nullptr, nullptr);
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
                       bool output_float,
                       std::uint16_t output_channels,
                       std::uint32_t sample_rate_hz,
                       std::uint32_t output_sample_rate_hz,
                       BoundedPcmQueue* queue,
                       std::atomic<bool>* stop_flag,
                       std::atomic<std::uint64_t>* write_error_count,
                       std::atomic<std::uint64_t>* chunks_pushed)
        : client_(client), render_(render),
          frame_size_bytes_(frame_size_bytes),
          output_float_(output_float),
          output_channels_(output_channels),
          sample_rate_hz_(sample_rate_hz),
          output_sample_rate_hz_(output_sample_rate_hz),
          queue_(queue), stop_flag_(stop_flag),
          write_error_count_(write_error_count),
          chunks_pushed_(chunks_pushed),
          chunker_(std::chrono::milliseconds(20), sample_rate_hz) {}

    void run() {
        const std::size_t chunk_samples = chunker_.chunk_samples();
        UpsampleState up_state;
        // The RC003 source is 16 kHz. Upsample only when the device
        // negotiated a different rate (48 kHz fallback); writing
        // source-rate samples into a 48 kHz client plays 3x fast and
        // the recognizer receives garbage.
        const bool need_upsample = (output_sample_rate_hz_ != sample_rate_hz_);
        UINT32 buffer_size = 0;
        client_->GetBufferSize(&buffer_size);
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
            // Pace to the device clock: wait until the engine has played
            // enough padding for this chunk to fit. Bursting into a large
            // shared buffer both overflows it and starves the mix.
            const UINT32 frames =
                static_cast<UINT32>(write_samples.size());
            UINT32 padding = 0;
            client_->GetCurrentPadding(&padding);
            while (padding + frames > buffer_size && !stop_flag_->load()) {
                Sleep(1);
                client_->GetCurrentPadding(&padding);
            }
            if (stop_flag_->load()) {
                break;
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
            write_error_count_->fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (output_float_) {
            // Negotiated container is IEEE float32 (the shared-mode
            // engine's native format). Convert once here and duplicate
            // mono into every negotiated channel; the engine then
            // forwards without another conversion layer.
            std::vector<float> converted(
                static_cast<std::size_t>(frames) * output_channels_);
            for (UINT32 i = 0; i < frames; ++i) {
                const float v =
                    static_cast<float>(samples[i]) / 32768.0f;
                for (std::uint16_t c = 0; c < output_channels_; ++c) {
                    converted[static_cast<std::size_t>(i) * output_channels_ +
                              c] = v;
                }
            }
            std::memcpy(data, converted.data(),
                        converted.size() * sizeof(float));
        } else if (output_channels_ == 2) {
            std::vector<std::int16_t> converted(
                static_cast<std::size_t>(frames) * 2);
            for (UINT32 i = 0; i < frames; ++i) {
                converted[static_cast<std::size_t>(i) * 2] = samples[i];
                converted[static_cast<std::size_t>(i) * 2 + 1] = samples[i];
            }
            std::memcpy(data, converted.data(),
                        converted.size() * sizeof(std::int16_t));
        } else {
            std::memcpy(data, samples.data(),
                        static_cast<std::size_t>(frames) *
                        static_cast<std::size_t>(frame_size_bytes_));
        }
        if (render_->ReleaseBuffer(frames, 0) != S_OK) {
            write_error_count_->fetch_add(1, std::memory_order_relaxed);
        } else if (chunks_pushed_ != nullptr) {
            chunks_pushed_->fetch_add(1, std::memory_order_relaxed);
        }
    }

    IAudioClient* client_;
    IAudioRenderClient* render_;
    UINT32 frame_size_bytes_;
    bool output_float_;
    std::uint16_t output_channels_;
    std::uint32_t sample_rate_hz_;
    std::uint32_t output_sample_rate_hz_;
    BoundedPcmQueue* queue_;
    std::atomic<bool>* stop_flag_;
    std::atomic<std::uint64_t>* write_error_count_;
    std::atomic<std::uint64_t>* chunks_pushed_;
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
    if (running_.load() || writer_thread_.joinable() || pcm_queue_ != nullptr
        || client_ != nullptr || render_ != nullptr || device_ != nullptr) {
        last_error_ = "already started";
        return false;  // already started
    }
    current_format_ = format;
    output_sample_rate_hz_ = format.sample_rate;

    // Step 1: enumerate + resolve endpoint.
    IMMDeviceEnumerator* enumerator = nullptr;
    if (CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator),
                         reinterpret_cast<void**>(&enumerator)) != S_OK) {
        last_error_ = "CoCreateInstance(MMDeviceEnumerator) failed (no COM on calling thread)";
        return false;
    }
    IMMDeviceCollection* collection = nullptr;
    if (enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
                                       &collection) != S_OK) {
        last_error_ = "EnumAudioEndpoints failed";
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
        last_error_ = "render endpoint not found or inactive: " +
                      utf16_to_utf8(endpoint_name_);
        return false;
    }
    {
        LPWSTR device_id = nullptr;
        if (matched_device->GetId(&device_id) == S_OK && device_id != nullptr) {
            matched_endpoint_id_ = utf16_to_utf8(device_id);
            CoTaskMemFree(device_id);
        }
    }

    // Step 2: activate IAudioClient.
    IAudioClient* audio_client = nullptr;
    if (matched_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(&audio_client)) != S_OK) {
        last_error_ = "IAudioClient Activate failed";
        matched_device->Release();
        return false;
    }

    // Step 3: run the stream at the device mix format, exactly like
    // PortAudio's shared-mode path. GetMixFormat (not IsFormatSupported)
    // names the engine's native stream format; VB-Cable passes float32
    // mix streams while negotiated int16 sessions stay silent. The
    // writer converts the 16 kHz int16 source into this format:
    // up-sampling to the mix rate, float conversion when the container
    // is 32-bit, and mono duplicated into every mix channel.
    WAVEFORMATEX* mix_format = nullptr;
    if (audio_client->GetMixFormat(&mix_format) != S_OK || mix_format == nullptr) {
        audio_client->Release();
        matched_device->Release();
        last_error_ = "GetMixFormat failed";
        return false;
    }
    const std::uint16_t output_channels = mix_format->nChannels;
    const bool output_float = mix_format->wBitsPerSample == 32;
    const UINT32 output_frame_size_bytes =
        (mix_format->wBitsPerSample / 8) * mix_format->nChannels;
    output_sample_rate_hz_ = mix_format->nSamplesPerSec;
    mix_channels_ = output_channels;
    mix_bits_per_sample_ = mix_format->wBitsPerSample;
    mix_is_float_ = output_float;

    // Step 4: initialize + start. hnsBufferDuration=0 lets the engine
    // pick the default instead of forcing 50 ms. The stream runs at the
    // device mix format with PortAudio's shared-mode stream flags.
    const DWORD shared_flags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                               AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    if (audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, shared_flags,
                                 0, 0, mix_format, nullptr) != S_OK) {
        CoTaskMemFree(mix_format);
        audio_client->Release();
        matched_device->Release();
        last_error_ = "IAudioClient Initialize failed";
        return false;
    }
    CoTaskMemFree(mix_format);
    if (audio_client->Start() != S_OK) {
        audio_client->Release();
        matched_device->Release();
        last_error_ = "IAudioClient Start failed";
        return false;
    }

    IAudioRenderClient* render_client = nullptr;
    if (audio_client->GetService(__uuidof(IAudioRenderClient),
                                 reinterpret_cast<void**>(&render_client)) != S_OK) {
        audio_client->Stop();
        audio_client->Release();
        matched_device->Release();
        last_error_ = "GetService(IAudioRenderClient) failed";
        return false;
    }

    // Diagnostic: capture the session volume so a zero/muted session
    // (the classic silent-shared-stream failure) is visible.
    ISimpleAudioVolume* session_volume = nullptr;
    if (audio_client->GetService(__uuidof(ISimpleAudioVolume),
                                 reinterpret_cast<void**>(&session_volume)) == S_OK
        && session_volume != nullptr) {
        float volume = 0.0f;
        BOOL muted = FALSE;
        session_volume->GetMasterVolume(&volume);
        session_volume->GetMute(&muted);
        session_volume->Release();
        session_volume_info_ = "volume=" + std::to_string(volume) +
                               " muted=" + (muted ? "yes" : "no");
    } else {
        session_volume_info_ = "volume unknown";
    }

    // Step 5: spin up the bounded queue + writer jthread.
    auto* queue = new BoundedPcmQueue(
        format.sample_rate * DEFAULT_QUEUE_CAPACITY_SECONDS);
    pcm_queue_ = queue;

    stop_requested_.store(false, std::memory_order_release);

    auto* writer = new WasapiWriterThread(
        audio_client, render_client,
        /*frame_size_bytes=*/output_frame_size_bytes,
        /*output_float=*/output_float,
        /*output_channels=*/output_channels,
        /*source_sample_rate_hz=*/format.sample_rate,
        /*output_sample_rate_hz=*/output_sample_rate_hz_,
        queue, &stop_requested_, &write_error_count_, &chunks_pushed_);
    auto thread = std::thread([writer]() { writer->run(); });
    writer_thread_ = std::move(thread);

    device_ = matched_device;
    client_ = audio_client;
    render_ = render_client;
    running_.store(true);
    last_error_.clear();
    return true;
#else
    (void)format;
    last_error_ = "WASAPI is Windows-only";
    return false;  // non-Windows: no real WASAPI
#endif
}

std::string WasapiAudioRoute::last_error() const {
    return last_error_;
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
    running_.store(false);
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
    return write_error_count_.load(std::memory_order_relaxed);
}

std::uint64_t WasapiAudioRoute::chunks_pushed_count() const noexcept {
    return chunks_pushed_.load(std::memory_order_relaxed);
}

std::uint32_t WasapiAudioRoute::output_sample_rate_hz() const noexcept {
    return output_sample_rate_hz_;
}

std::string WasapiAudioRoute::matched_endpoint_id() const noexcept {
    return matched_endpoint_id_;
}

std::uint16_t WasapiAudioRoute::mix_channels() const noexcept {
    return mix_channels_;
}

std::uint16_t WasapiAudioRoute::mix_bits_per_sample() const noexcept {
    return mix_bits_per_sample_;
}

bool WasapiAudioRoute::mix_is_float() const noexcept {
    return mix_is_float_;
}

std::string WasapiAudioRoute::session_volume_info() const noexcept {
    return session_volume_info_;
}

bool WasapiAudioRoute::writer_thread_alive() const noexcept {
    return writer_thread_.joinable();
}

PcmFormat WasapiAudioRoute::current_format() const noexcept {
    return current_format_;
}

} // namespace remotemic::audio

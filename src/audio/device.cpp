// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "audio/device.h"

#include <portaudio.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include "common.h"

namespace ocs::audio {

namespace {
std::atomic<int> g_pa_refs{0};
std::mutex g_pa_mutex;
}  // namespace

bool pa_init() {
    std::lock_guard<std::mutex> lock(g_pa_mutex);
    if (g_pa_refs.load() > 0) {
        ++g_pa_refs;
        return true;
    }
    const PaError e = Pa_Initialize();
    if (e != paNoError) return false;
    g_pa_refs.store(1);
    return true;
}

void pa_shutdown() {
    std::lock_guard<std::mutex> lock(g_pa_mutex);
    if (g_pa_refs.load() <= 0) return;
    if (--g_pa_refs == 0) {
        Pa_Terminate();
    }
}

const char* pa_last_error() {
    const PaHostErrorInfo* info = Pa_GetLastHostErrorInfo();
    return info ? info->errorText : "unknown PortAudio error";
}

std::vector<DeviceInfo> list_devices() {
    std::vector<DeviceInfo> out;
    const int n = Pa_GetDeviceCount();
    if (n < 0) return out;
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info) continue;
        DeviceInfo d;
        d.index = i;
        d.name = info->name ? info->name : "";
        d.max_inputs = info->maxInputChannels;
        d.max_outputs = info->maxOutputChannels;
        d.default_sample_rate = info->defaultSampleRate;
        out.push_back(std::move(d));
    }
    return out;
}

namespace {

// ALSA pseudo-plugins / virtual graph nodes that are not real capture or
// playback hardware. "default" is kept because it routes to the real device.
bool is_pseudo_device(const std::string& name) {
    static const char* kPseudo[] = {
        "front",     "surround40", "surround41", "surround50",
        "surround51", "surround71", "dmix",      "dsnoop",
        "null",      "sysdefault", "pipewire",   "pulse",
        "jack",      "empty",      "speexrate",
    };
    for (const char* p : kPseudo) {
        if (name == p) return true;
    }
    return false;
}

}  // namespace

std::vector<DeviceInfo> list_input_devices() {
    const auto all = list_devices();
    const int def = default_input_device();
    std::vector<DeviceInfo> out;
    for (const auto& d : all) {
        if (d.max_inputs <= 0) continue;
        if (d.index == def) {
            out.push_back(d);
            continue;
        }
        if (is_pseudo_device(d.name)) continue;
        out.push_back(d);
    }
    return out;
}

std::vector<DeviceInfo> list_output_devices() {
    const auto all = list_devices();
    const int def = default_output_device();
    std::vector<DeviceInfo> out;
    for (const auto& d : all) {
        if (d.max_outputs <= 0) continue;
        if (d.index == def) {
            out.push_back(d);
            continue;
        }
        if (is_pseudo_device(d.name)) continue;
        out.push_back(d);
    }
    return out;
}

int default_input_device() {
    const PaDeviceIndex i = Pa_GetDefaultInputDevice();
    return i == paNoDevice ? -1 : i;
}

int default_output_device() {
    const PaDeviceIndex i = Pa_GetDefaultOutputDevice();
    return i == paNoDevice ? -1 : i;
}

int default_sample_rate() {
    const PaDeviceIndex i = Pa_GetDefaultInputDevice();
    if (i != paNoDevice) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info && info->defaultSampleRate > 0) {
            return static_cast<int>(info->defaultSampleRate);
        }
    }
    return 48000;
}

LiveStream::LiveStream() : input_rb(1 << 18), output_rb(1 << 18) {}

namespace {

// Real-time callback: mic -> input ring; output ring -> speakers.
int live_cb(const void* in, void* out, unsigned long nframes,
            const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags,
            void* user) {
    LiveStream* ls = static_cast<LiveStream*>(user);
    const float* in_f = static_cast<const float*>(in);
    float* out_f = static_cast<float*>(out);

    if (in_f) ls->input_rb.write(in_f, nframes);

    const size_t got = ls->output_rb.read(out_f, nframes);
    for (size_t i = got; i < nframes; ++i) out_f[i] = 0.0f;  // underflow -> silence
    return paContinue;
}

}  // namespace

bool open_live(LiveStream& ls, int input_dev, int output_dev,
               int sample_rate, size_t frames_per_buffer, std::string& err) {
    ls.sample_rate = sample_rate;
    ls.frames_per_buffer = frames_per_buffer;
    ls.input_device = input_dev;
    ls.output_device = output_dev;
    ls.input_rb.reset();
    ls.output_rb.reset();

    PaStreamParameters in_params{};
    in_params.device = input_dev;
    in_params.channelCount = 1;
    in_params.sampleFormat = paFloat32;
    in_params.suggestedLatency =
        Pa_GetDeviceInfo(input_dev)
            ? Pa_GetDeviceInfo(input_dev)->defaultLowInputLatency
            : 0.05;

    PaStreamParameters out_params{};
    out_params.device = output_dev;
    out_params.channelCount = 1;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency =
        Pa_GetDeviceInfo(output_dev)
            ? Pa_GetDeviceInfo(output_dev)->defaultLowOutputLatency
            : 0.05;

    PaError e = Pa_OpenStream(
        &ls.stream, &in_params, &out_params, sample_rate,
        static_cast<unsigned long>(frames_per_buffer), paClipOff,
        live_cb, &ls);
    if (e != paNoError) {
        const char* in_name = Pa_GetDeviceInfo(input_dev) ? Pa_GetDeviceInfo(input_dev)->name : "?";
        const char* out_name = Pa_GetDeviceInfo(output_dev) ? Pa_GetDeviceInfo(output_dev)->name : "?";
        err = std::string(Pa_GetErrorText(e)) + " (input '" + in_name +
              "', output '" + out_name +
              "'). Is the microphone in use by another application?";
        return false;
    }
    return true;
}

bool start_live(LiveStream& ls) {
    if (!ls.stream) return false;
    PaError e = Pa_StartStream(static_cast<PaStream*>(ls.stream));
    if (e != paNoError) return false;
    ls.running.store(true);
    return true;
}

bool stop_live(LiveStream& ls) {
    if (!ls.stream) return false;
    PaError e = Pa_StopStream(static_cast<PaStream*>(ls.stream));
    ls.running.store(false);
    return e == paNoError;
}

void close_live(LiveStream& ls) {
    if (ls.stream) {
        Pa_CloseStream(static_cast<PaStream*>(ls.stream));
        ls.stream = nullptr;
    }
}

void play_buffer(const AudioData& data, int out_device, std::string* err) {
    if (data.samples.empty() || out_device < 0 || data.sample_rate <= 0) {
        if (err) *err = "nothing to play";
        return;
    }

    PaStreamParameters out_params{};
    out_params.device = out_device;
    out_params.channelCount = 1;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency =
        Pa_GetDeviceInfo(out_device)
            ? Pa_GetDeviceInfo(out_device)->defaultHighOutputLatency
            : 0.1;

    PaStream* stream = nullptr;
    PaError e = Pa_OpenStream(
        &stream, nullptr, &out_params, data.sample_rate,
        paFramesPerBufferUnspecified, paClipOff, nullptr, nullptr);
    if (e != paNoError) {
        if (err) *err = Pa_GetErrorText(e);
        return;
    }

    std::vector<float> out_f(data.samples.size());
    for (size_t i = 0; i < data.samples.size(); ++i) {
        out_f[i] = static_cast<float>(data.samples[i]);
    }

    if (Pa_StartStream(stream) != paNoError) {
        Pa_CloseStream(stream);
        if (err) *err = "could not start playback stream";
        return;
    }

    const size_t chunk = 4096;
    size_t pos = 0;
    while (pos < out_f.size()) {
        const unsigned long n = static_cast<unsigned long>(
            std::min(chunk, out_f.size() - pos));
        e = Pa_WriteStream(stream, out_f.data() + pos, n);
        if (e != paNoError) {
            if (err) *err = Pa_GetErrorText(e);
            break;
        }
        pos += n;
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
}

AudioData record_seconds(double seconds, int in_device, int sample_rate,
                         std::string* err) {
    if (in_device < 0 || seconds <= 0) {
        if (err) *err = "invalid recording parameters";
        return {};
    }

    PaStreamParameters in_params{};
    in_params.device = in_device;
    in_params.channelCount = 1;
    in_params.sampleFormat = paFloat32;
    in_params.suggestedLatency =
        Pa_GetDeviceInfo(in_device)
            ? Pa_GetDeviceInfo(in_device)->defaultHighInputLatency
            : 0.1;

    PaStream* stream = nullptr;
    PaError e = Pa_OpenStream(
        &stream, &in_params, nullptr, sample_rate,
        paFramesPerBufferUnspecified, paClipOff, nullptr, nullptr);
    if (e != paNoError) {
        if (err) *err = Pa_GetErrorText(e);
        return {};
    }

    AudioData out;
    out.sample_rate = sample_rate;
    const size_t total = static_cast<size_t>(seconds * sample_rate);
    out.samples.reserve(total);

    if (Pa_StartStream(stream) != paNoError) {
        Pa_CloseStream(stream);
        if (err) *err = "could not start recording stream";
        return {};
    }

    const size_t chunk = 4096;
    std::vector<float> buf(chunk);
    size_t got = 0;
    while (got < total) {
        const unsigned long n = static_cast<unsigned long>(
            std::min(chunk, total - got));
        e = Pa_ReadStream(stream, buf.data(), n);
        if (e != paNoError) {
            if (err) *err = Pa_GetErrorText(e);
            break;
        }
        for (unsigned long i = 0; i < n; ++i) {
            out.samples.push_back(static_cast<double>(buf[i]));
        }
        got += n;
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    return out;
}

}  // namespace ocs::audio

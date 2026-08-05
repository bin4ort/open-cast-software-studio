// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#include "audio/device.h"

#include <portaudio.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>

namespace va::audio {

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
        err = Pa_GetErrorText(e);
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

}  // namespace va::audio

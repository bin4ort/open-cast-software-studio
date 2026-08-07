// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "audio/virtualmic.h"

#include <portaudio.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "audio/device.h"

namespace ocs::audio {

namespace {

std::string run_capture(const std::string& cmd) {
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return out;
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
    pclose(p);
    return out;
}

// Scans PortAudio for a snd-aloop playback device. Returns its index or -1.
int find_loopback_output() {
    const auto devices = list_devices();
    for (const auto& d : devices) {
        if (d.max_outputs > 0 &&
            d.name.find("Loopback") != std::string::npos) {
            return d.index;
        }
    }
    return -1;
}

}  // namespace

VirtualMicType probe_virtual_mic(std::string& display_name) {
    // 1. PipeWire/PulseAudio pipe-source.
    const std::string module = run_capture(
        "pactl load-module module-pipe-source "
        "source_name=ocs_probe file=/tmp/ocs_probe.fifo "
        "format=s16le rate=48000 channels=1 2>/dev/null");
    if (!module.empty() && module.find_first_of("0123456789") != std::string::npos) {
        std::system(("pactl unload-module " + module + " 2>/dev/null").c_str());
        std::remove("/tmp/ocs_probe.fifo");
        display_name = "OCS Virtual Mic (PipeWire)";
        return VirtualMicType::PipeSource;
    }
    std::remove("/tmp/ocs_probe.fifo");

    // 2. snd-aloop kernel loopback.
    if (find_loopback_output() >= 0) {
        display_name = "snd-aloop loopback";
        return VirtualMicType::AlsaLoopback;
    }

    display_name = "none";
    return VirtualMicType::None;
}

VirtualMic::~VirtualMic() {
    destroy();
}

bool VirtualMic::create(int sample_rate, std::string& err) {
    if (active_) return true;
    if (create_pipe_source(sample_rate, err)) return true;
    if (create_alsa_loopback(sample_rate, err)) return true;
    err = "no virtual microphone available. Install/load snd-aloop "
          "(sudo modprobe snd-aloop) or ensure PipeWire/PulseAudio is "
          "running.";
    return false;
}

bool VirtualMic::create_pipe_source(int sample_rate, std::string& err) {
    // Create the FIFO, then expose it to PulseAudio/PipeWire as a source.
    fifo_path_ = "/tmp/ocs_virtual_mic.fifo";
    std::remove(fifo_path_.c_str());
    if (mkfifo(fifo_path_.c_str(), 0600) != 0) {
        fifo_path_.clear();
        return false;
    }

    const std::string cmd =
        "pactl load-module module-pipe-source "
        "source_name=ocs_virtual_mic file=" + fifo_path_ +
        " format=s16le rate=" + std::to_string(sample_rate) +
        " channels=1 2>/dev/null";
    const std::string module = run_capture(cmd);
    const bool ok = !module.empty() &&
                    module.find_first_of("0123456789") != std::string::npos;
    if (!ok) {
        std::remove(fifo_path_.c_str());
        fifo_path_.clear();
        err = "pactl module-pipe-source not supported by this audio server";
        return false;
    }
    module_id_ = module;
    display_name_ = "OCS Virtual Mic";
    type_ = VirtualMicType::PipeSource;

    // Open the FIFO read-write so writes never block even with no reader.
    fifo_fd_ = ::open(fifo_path_.c_str(), O_RDWR | O_NONBLOCK);
    if (fifo_fd_ < 0) {
        destroy();
        err = "could not open virtual mic FIFO";
        return false;
    }
    active_ = true;
    return true;
}

bool VirtualMic::create_alsa_loopback(int sample_rate, std::string& err) {
    const int dev = find_loopback_output();
    if (dev < 0) return false;

    PaStreamParameters out_params{};
    out_params.device = dev;
    out_params.channelCount = 1;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency =
        Pa_GetDeviceInfo(dev) ? Pa_GetDeviceInfo(dev)->defaultHighOutputLatency
                              : 0.05;

    PaStream* stream = nullptr;
    PaError e = Pa_OpenStream(&stream, nullptr, &out_params, sample_rate,
                              paFramesPerBufferUnspecified, paClipOff,
                              nullptr, nullptr);
    if (e != paNoError) {
        err = std::string("could not open loopback device: ") + Pa_GetErrorText(e);
        return false;
    }
    if (Pa_StartStream(stream) != paNoError) {
        Pa_CloseStream(stream);
        err = "could not start loopback stream";
        return false;
    }

    loop_stream_ = stream;
    loop_device_ = dev;
    display_name_ = "snd-aloop loopback";
    type_ = VirtualMicType::AlsaLoopback;
    active_ = true;
    return true;
}

void VirtualMic::destroy() {
    if (!active_) {
        return;
    }
    if (type_ == VirtualMicType::PipeSource) {
        if (fifo_fd_ >= 0) {
            ::close(fifo_fd_);
            fifo_fd_ = -1;
        }
        if (!module_id_.empty()) {
            std::system(("pactl unload-module " + module_id_ +
                         " 2>/dev/null").c_str());
            module_id_.clear();
        }
        std::remove(fifo_path_.c_str());
        fifo_path_.clear();
    } else if (type_ == VirtualMicType::AlsaLoopback) {
        if (loop_stream_) {
            Pa_StopStream(static_cast<PaStream*>(loop_stream_));
            Pa_CloseStream(static_cast<PaStream*>(loop_stream_));
            loop_stream_ = nullptr;
        }
    }
    active_ = false;
    type_ = VirtualMicType::None;
}

void VirtualMic::write(const float* samples, size_t n) {
    if (!active_ || n == 0) return;

    if (type_ == VirtualMicType::PipeSource) {
        s16buf_.resize(n);
        for (size_t i = 0; i < n; ++i) {
            float v = samples[i];
            v = std::max(-1.0f, std::min(1.0f, v));
            s16buf_[i] = static_cast<int16_t>(v * 32767.0f);
        }
        const ssize_t w = ::write(fifo_fd_, s16buf_.data(),
                                  s16buf_.size() * sizeof(int16_t));
        (void)w;  // partial/dropped writes are acceptable
    } else if (type_ == VirtualMicType::AlsaLoopback && loop_stream_) {
        PaError e = Pa_WriteStream(static_cast<PaStream*>(loop_stream_),
                                   samples, static_cast<unsigned long>(n));
        if (e == paOutputUnderflowed || e == paNoError) {
            return;
        }
        // Device busy / disconnected: drop until it recovers.
    }
}

}  // namespace ocs::audio

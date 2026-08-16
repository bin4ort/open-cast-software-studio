// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ai/regen.h"

#include <cstdio>
#include <filesystem>

#include <samplerate.h>
#include <unistd.h>

#include "audio/fileio.h"
#include "dsp/pipeline.h"
#include "transcribe/whisper.h"
#include "tts.h"

namespace ocs::ai {

namespace {

std::vector<double> resample_to(const std::vector<double>& in, int from_sr,
                                int to_sr) {
    if (from_sr <= 0 || from_sr == to_sr || in.empty()) return in;
    const double ratio = static_cast<double>(to_sr) / static_cast<double>(from_sr);
    const size_t out_cap = static_cast<size_t>(in.size() * ratio) + 64;
    std::vector<float> in_f(in.size()), out_f(out_cap);
    for (size_t i = 0; i < in.size(); ++i) in_f[i] = static_cast<float>(in[i]);
    SRC_DATA sd{};
    sd.data_in = in_f.data();
    sd.input_frames = static_cast<long>(in_f.size());
    sd.data_out = out_f.data();
    sd.output_frames = static_cast<long>(out_cap);
    sd.src_ratio = ratio;
    sd.end_of_input = 1;
    if (src_simple(&sd, SRC_SINC_MEDIUM_QUALITY, 1) != 0) return in;
    std::vector<double> out(static_cast<size_t>(sd.output_frames_gen));
    for (size_t i = 0; i < out.size(); ++i) out[i] = static_cast<double>(out_f[i]);
    return out;
}

}  // namespace

Regenerator::~Regenerator() {
    stop();
}

void Regenerator::configure(const std::string& model, dsp::Gender gender,
                            const dsp::ShiftParams& shift, int sample_rate) {
    std::lock_guard<std::mutex> lock(mu_);
    model_ = model;
    gender_ = gender;
    shift_ = shift;
    sample_rate_ = sample_rate > 0 ? sample_rate : 48000;
    configured_ = !model_.empty();
    if (thread_.joinable()) return;
    thread_ = std::thread(&Regenerator::worker, this);
}

bool Regenerator::ready() const {
    std::lock_guard<std::mutex> lock(mu_);
    return configured_ && !transcribe::find_whisper_binary().empty();
}

void Regenerator::submit(const std::vector<float>& samples) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (samples.empty() || !configured_) return;
        pending_.push_back(samples);
    }
    cv_.notify_one();
}

size_t Regenerator::poll(std::vector<float>& out, size_t max) {
    std::lock_guard<std::mutex> lock(mu_);
    const size_t take = std::min(max, output_.size());
    out.reserve(take);
    for (size_t i = 0; i < take; ++i) {
        out.push_back(output_.front());
        output_.pop_front();
    }
    return take;
}

void Regenerator::stop() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (stop_) return;
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void Regenerator::worker() {
    while (true) {
        std::vector<float> utterance;
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [this] { return stop_ || !pending_.empty(); });
            if (stop_ && pending_.empty()) return;
            utterance = std::move(pending_.front());
            pending_.pop_front();
        }

        // Save the utterance to a temp wav for whisper (unique name so
        // concurrent sessions never clobber each other).
        std::string tmp;
        {
            const auto dir = std::filesystem::temp_directory_path();
            std::string tmpl = (dir / "ocs_regen_XXXXXX.wav").string();
            std::vector<char> buf(tmpl.begin(), tmpl.end());
            buf.push_back('\0');
            const int fd = mkstemps(buf.data(), 4);
            if (fd < 0) continue;
            ::close(fd);
            tmp = buf.data();
        }
        {
            AudioData ad;
            ad.sample_rate = sample_rate_;
            ad.samples.reserve(utterance.size());
            for (float v : utterance) ad.samples.push_back(static_cast<Sample>(v));
            std::string werr;
            if (!audio::write_file(tmp, ad, &werr)) {
                std::error_code ec;
                std::filesystem::remove(tmp, ec);
                continue;
            }
        }

        transcribe::WhisperConfig wc;
        wc.model = model_;
        std::vector<Segment> segs;
        std::string terr;
        if (!transcribe::transcribe(tmp, wc, segs, terr)) {
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            continue;
        }

        std::string text;
        for (const auto& s : segs) {
            text += s.text;
            if (!text.empty() && text.back() != ' ') text += ' ';
        }
        std::filesystem::remove(tmp);
        if (text.empty()) continue;

        AudioData spoken;
        std::string serr;
        if (!tts::synthesize(text, gender_, spoken, serr)) continue;

        std::vector<double> d(spoken.samples.begin(), spoken.samples.end());
        d = resample_to(d, spoken.sample_rate, sample_rate_);
        d = dsp::shift_audio(d, sample_rate_, shift_);

        std::lock_guard<std::mutex> lock(mu_);
        for (double v : d) {
            output_.push_back(static_cast<float>(v));
        }
    }
}

}  // namespace ocs::ai

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#include "audio/fileio.h"

#include <sndfile.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifdef VA_HAVE_FFMPEG
#include <unistd.h>
#endif

namespace va::audio {

namespace {

#ifdef VA_HAVE_FFMPEG
std::string temp_wav_path(std::string& err) {
    const auto dir = std::filesystem::temp_directory_path();
    const std::string tmpl = (dir / "va_XXXXXX.wav").string();
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    const int fd = mkstemps(buf.data(), 4);
    if (fd < 0) {
        err = "failed to create temp file";
        return {};
    }
    close(fd);
    return std::string(buf.data());
}
#endif

}  // namespace

bool sndfile_can_handle(const std::string& path) {
    SF_INFO info{};
    SNDFILE* f = sf_open(path.c_str(), SFM_READ, &info);
    if (f) {
        sf_close(f);
        return true;
    }
    return false;
}

bool ffmpeg_available() {
#ifdef VA_HAVE_FFMPEG
    FILE* p = popen("ffmpeg -version >/dev/null 2>&1", "r");
    if (!p) return false;
    const int rc = pclose(p);
    return rc == 0;
#else
    return false;
#endif
}

AudioData read_file(const std::string& path, std::string* err) {
    if (!sndfile_can_handle(path)) {
        if (ffmpeg_available()) {
            std::string e;
            const std::string wav = temp_wav_path(e);
            if (wav.empty()) {
                if (err) *err = e;
                return {};
            }
#ifdef VA_HAVE_FFMPEG
            if (system(("ffmpeg -y -v error -i \"" + path +
                        "\" -ac 1 -ar 48000 \"" + wav + "\"")
                           .c_str()) != 0) {
                if (err) *err = "ffmpeg failed to decode '" + path + "'";
                return {};
            }
            AudioData ad = read_file(wav, err);
            std::remove(wav.c_str());
            return ad;
#endif
        }
        if (err) *err = "unsupported format (and no ffmpeg available): " + path;
        return {};
    }

    SF_INFO info{};
    SNDFILE* f = sf_open(path.c_str(), SFM_READ, &info);
    if (!f) {
        if (err) *err = std::string("sndfile: ") + sf_strerror(nullptr);
        return {};
    }

    AudioData ad;
    ad.sample_rate = info.samplerate;
    const sf_count_t total = info.frames * info.channels;
    std::vector<float> inter(total);
    const sf_count_t read = sf_readf_float(f, inter.data(), info.frames);
    sf_close(f);

    if (read <= 0) return ad;
    const sf_count_t frames = read;
    ad.samples.reserve(static_cast<size_t>(frames));
    for (sf_count_t i = 0; i < frames; ++i) {
        float v = 0.0f;
        for (int c = 0; c < info.channels; ++c) {
            v += inter[i * info.channels + c];
        }
        v /= static_cast<float>(info.channels);
        ad.samples.push_back(v);
    }
    return ad;
}

bool write_file(const std::string& path, const AudioData& data,
                std::string* err) {
    if (data.samples.empty() || data.sample_rate <= 0) {
        if (err) *err = "no audio to write";
        return false;
    }

    auto ext = std::filesystem::path(path).extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    int format = 0;
    if (ext == ".wav") format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    else if (ext == ".flac") format = SF_FORMAT_FLAC | SF_FORMAT_PCM_16;
    else if (ext == ".ogg") format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
    else if (ext == ".aiff" || ext == ".aif") format = SF_FORMAT_AIFF | SF_FORMAT_PCM_16;
    else {
        if (ffmpeg_available()) {
#ifdef VA_HAVE_FFMPEG
            std::string e;
            const std::string wav = temp_wav_path(e);
            if (wav.empty()) {
                if (err) *err = e;
                return false;
            }
            AudioData tmp = data;
            if (!write_file(wav, tmp, err)) return false;
            const int rc = system(("ffmpeg -y -v error -i \"" + wav + "\" \"" + path + "\"").c_str());
            std::remove(wav.c_str());
            if (rc != 0) {
                if (err) *err = "ffmpeg failed to encode '" + path + "'";
                return false;
            }
            return true;
#endif
        }
        if (err) *err = "unsupported output format: " + path;
        return false;
    }

    SF_INFO info{};
    info.samplerate = data.sample_rate;
    info.channels = 1;
    info.format = format;
    SNDFILE* f = sf_open(path.c_str(), SFM_WRITE, &info);
    if (!f) {
        if (err) *err = std::string("sndfile: ") + sf_strerror(nullptr);
        return false;
    }
    const sf_count_t written =
        sf_writef_float(f, data.samples.data(), static_cast<sf_count_t>(data.samples.size()));
    sf_close(f);
    if (written != static_cast<sf_count_t>(data.samples.size())) {
        if (err) *err = "short write to " + path;
        return false;
    }
    return true;
}

}  // namespace va::audio

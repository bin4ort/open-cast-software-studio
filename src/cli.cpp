// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "cli.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "ai/llm.h"
#include "ai/vc.h"
#include "anonymize/redactor.h"
#include "audio/device.h"
#include "audio/fileio.h"
#include "dsp/phase_vocoder.h"
#include "dsp/pipeline.h"
#include "dsp/vad.h"
#include "processing/job.h"
#include "transcribe/whisper.h"

namespace ocs::cli {

namespace {

int g_argc = 0;
char** g_argv = nullptr;

void print_version() {
    std::printf("ocs_studio %s (GPL-3.0-only)\n", OCS_VERSION);
    std::printf("Built with g++ %d.%d.%d\n", __GNUC__, __GNUC_MINOR__,
                __GNUC_PATCHLEVEL__);
#ifdef OCS_HAVE_FFMPEG
    std::printf("ffmpeg subprocess: enabled\n");
#else
    std::printf("ffmpeg subprocess: unavailable (recompile with ffmpeg found)\n");
#endif
    if (transcribe::find_whisper_binary().empty()) {
        std::printf("whisper: not found (transcription disabled)\n");
    } else {
        std::printf("whisper: %s\n", transcribe::find_whisper_binary().c_str());
    }
    std::printf("neural VC: %s\n", vc::status().c_str());
    std::printf("local LLM: %s\n",
                llm::llm_available() ? llm::backend_name().c_str()
                                     : "not detected");
    std::printf("  %s\n", llm::status_text().c_str());
}

void print_help() {
    std::printf(
        "ocs_studio %s - voice anonymization toolkit (GPL-3.0-only)\n\n"
        "Usage:\n"
        "  ocs_studio process <input> [options]   Anonymize an audio file\n"
        "  ocs_studio live [options]              Real-time microphone masking\n"
        "  ocs_studio transcribe <input> [opts]   Extract a transcript\n"
        "  ocs_studio anonymize <transcript> [o]  Redact a transcript file\n"
        "  ocs_studio presets                     List shift presets\n"
        "  ocs_studio devices                     List audio devices\n"
        "  ocs_studio info                        Show build/runtime info\n"
        "  ocs_studio gui                         Launch the Qt GUI (if built)\n"
        "  ocs_studio help                        Show this help\n"
        "\n"
        "process options:\n"
        "  -o <path>              Output file (default: <input>.anon.ext)\n"
        "  --preset <name>        Shift preset (see `presets`)\n"
        "  --pitch <semitones>    Pitch shift (default 2.0)\n"
        "  --formant <semitones>  Final formant position (0 preserves formants)\n"
        "  --mode global|selective|hybrid  Shift all speech, only PII segments,\n"
        "                               or global gender shift + re-voiced behavior\n"
        "  --gender generic|male|female|custom  Target voice gender (hybrid)\n"
        "  --transcribe           Transcribe and redact along the way\n"
        "  --write-transcript     Write <output>.txt transcript\n"
        "  --synthetic            Replace names with plausible stand-ins\n"
        "  --replacement <str>    Text used for redacted tokens (default [REDACTED])\n"
        "  --denoise              Enable spectral denoising\n"
        "  --whisper <binary>     Whisper executable override\n"
        "  --model <path>         Whisper.cpp model path\n"
        "  --lang <code>          Transcription language (e.g. en)\n"
        "\n"
        "live options:\n"
        "  --in <device-index>    Input (mic) device; `devices` to list\n"
        "  --out <device-index>   Output (speaker) device\n"
        "  --preset <name>        Shift preset\n"
        "  --pitch <semitones>    Pitch shift\n"
        "  --rate <hz>            Sample rate (default device default)\n"
        "  --frames <n>           Frames per buffer (default 1024)\n"
        "  --mute-silence         Output silence when VAD says no speech\n"
        "  --dry                  Pass through without shifting (test routing)\n"
        "\n"
        "transcribe options:\n"
        "  -o <path>              Output file (default <input>.transcript.txt)\n"
        "  --redact               Redact PII in the transcript\n"
        "  --whisper <binary>     Whisper executable override\n"
        "  --model <path>         Whisper.cpp model path\n"
        "  --lang <code>          Language\n"
        "\n"
        "anonymize options:\n"
        "  -o <path>              Output file (default <input>.redacted.txt)\n"
        "  --synthetic            Use synthetic replacement names\n"
        "  --replacement <str>    Redaction token\n",
        OCS_VERSION);
}

// ---------------------------------------------------------------------------
// small option scanner
// ---------------------------------------------------------------------------
struct Argv {
    int argc;
    char** argv;
    int i = 1;

    bool done() const { return i >= argc; }
    const char* next() {
        return i < argc ? argv[i++] : "";
    }
    const char* peek() const {
        return i < argc ? argv[i] : "";
    }
};

// Consumes the value of `opt`, reporting a clear error when it is missing.
const char* opt_value(Argv& a, const char* opt) {
    const char* v = a.next();
    if (*v == '\0') {
        std::fprintf(stderr, "error: option %s requires a value\n", opt);
        return nullptr;
    }
    return v;
}

// String option variant that assigns directly into a std::string.
bool opt_string(Argv& a, const char* opt, std::string& out) {
    const char* v = opt_value(a, opt);
    if (!v) return false;
    out = v;
    return true;
}

// Parses a double option value, rejecting non-numeric/NaN/inf input.
bool opt_double(Argv& a, const char* opt, double& out) {
    const char* v = opt_value(a, opt);
    if (!v) return false;
    char* end = nullptr;
    const double d = std::strtod(v, &end);
    if (end == v || *end != '\0' || !std::isfinite(d)) {
        std::fprintf(stderr, "error: option %s requires a numeric value\n", opt);
        return false;
    }
    out = d;
    return true;
}

// Parses an integer option value (rejects garbage and non-positive values
// when `positive` is set).
bool opt_int(Argv& a, const char* opt, long& out, bool positive) {
    const char* v = opt_value(a, opt);
    if (!v) return false;
    char* end = nullptr;
    const long d = std::strtol(v, &end, 10);
    if (end == v || *end != '\0' || (positive && d <= 0)) {
        std::fprintf(stderr, "error: option %s requires an %s value\n", opt,
                     positive ? "integer > 0" : "integer");
        return false;
    }
    out = d;
    return true;
}

// ---------------------------------------------------------------------------
// process
// ---------------------------------------------------------------------------
int cmd_process(Argv& a) {
    const std::string input = a.next();
    if (input.empty()) {
        std::fprintf(stderr, "error: process requires an input file\n");
        return 1;
    }

    job::JobOptions opts;
    opts.input = input;
    dsp::ShiftParams shift;
    std::string preset;
    std::string mode;
    std::string gender;
    bool pitch_set = false, formant_set = false;
    bool write_transcript = false;
    bool transcribe = false;
    bool synthetic = false;
    std::string replacement;

    while (!a.done()) {
        const char* arg = a.peek();
        if (std::strcmp(arg, "-o") == 0) { a.next(); if (!opt_string(a, "-o", opts.output)) return 1; }
        else if (std::strcmp(arg, "--preset") == 0) { a.next(); if (!opt_string(a, "--preset", preset)) return 1; }
        else if (std::strcmp(arg, "--pitch") == 0) { a.next(); if (!opt_double(a, "--pitch", shift.pitch_semitones)) return 1; pitch_set = true; }
        else if (std::strcmp(arg, "--formant") == 0) { a.next(); if (!opt_double(a, "--formant", shift.formant_semitones)) return 1; formant_set = true; }
        else if (std::strcmp(arg, "--mode") == 0) { a.next(); if (!opt_string(a, "--mode", mode)) return 1; }
        else if (std::strcmp(arg, "--gender") == 0) { a.next(); if (!opt_string(a, "--gender", gender)) return 1; }
        else if (std::strcmp(arg, "--transcribe") == 0) { a.next(); transcribe = true; }
        else if (std::strcmp(arg, "--write-transcript") == 0) { a.next(); write_transcript = true; }
        else if (std::strcmp(arg, "--synthetic") == 0) { a.next(); synthetic = true; }
        else if (std::strcmp(arg, "--replacement") == 0) { a.next(); if (!opt_string(a, "--replacement", replacement)) return 1; }
        else if (std::strcmp(arg, "--denoise") == 0) { a.next(); shift.enable_denoise = true; }
        else if (std::strcmp(arg, "--whisper") == 0) { a.next(); if (!opt_string(a, "--whisper", opts.whisper_binary)) return 1; }
        else if (std::strcmp(arg, "--model") == 0) { a.next(); if (!opt_string(a, "--model", opts.whisper_model)) return 1; }
        else if (std::strcmp(arg, "--lang") == 0) { a.next(); if (!opt_string(a, "--lang", opts.whisper_language)) return 1; }
        else {
            std::fprintf(stderr, "error: unknown option '%s'\n", arg);
            return 1;
        }
    }

    // DSP shifts outside the supported range make the vocoder unstable
    // (extreme stretch factors) or throw; reject them up front like the GUI
    // does (sliders are limited to +-24 st).
    if (shift.pitch_semitones < -24.0 || shift.pitch_semitones > 24.0 ||
        shift.formant_semitones < -24.0 || shift.formant_semitones > 24.0) {
        std::fprintf(stderr,
                     "error: --pitch/--formant must be within [-24, 24] semitones\n");
        return 1;
    }

    if (!preset.empty()) {
        const dsp::Preset* p = dsp::preset_named(preset);
        if (!p) {
            std::fprintf(stderr, "error: unknown preset '%s'\n", preset.c_str());
            return 1;
        }
        shift = p->params;
    }
    // A target gender applies its profile unless pitch/formant were explicit.
    if (!gender.empty()) {
        const dsp::GenderProfile prof =
            dsp::profile_for(dsp::gender_from_name(gender.c_str()));
        if (!pitch_set) shift.pitch_semitones = prof.pitch_semitones;
        if (!formant_set) shift.formant_semitones = prof.formant_semitones;
    }
    opts.shift = shift;
    opts.mode = dsp::AnonMode::Global;
    if (!mode.empty()) {
        if (mode == "global") opts.mode = dsp::AnonMode::Global;
        else if (mode == "selective") opts.mode = dsp::AnonMode::Selective;
        else if (mode == "hybrid") opts.mode = dsp::AnonMode::Hybrid;
        else {
            std::fprintf(stderr,
                         "error: --mode must be global, selective or hybrid\n");
            return 1;
        }
    }
    opts.target_gender = dsp::gender_from_name(gender.c_str());
    opts.transcribe = transcribe;
    opts.write_transcript = write_transcript;
    opts.synthetic_names = synthetic;
    if (!replacement.empty()) opts.name_replacement = replacement;

    if (opts.output.empty()) {
        const auto dot = input.find_last_of('.');
        opts.output = (dot == std::string::npos ? input : input.substr(0, dot)) +
                      ".anon" + (dot == std::string::npos ? ".wav" : input.substr(dot));
    }

    const job::JobResult res = job::run(opts);
    if (!res.ok) {
        std::fprintf(stderr, "error: %s\n", res.message.c_str());
        return 1;
    }

    if (!res.redacted_path.empty()) {
        std::printf("wrote %s\n", res.redacted_path.c_str());
    }
    if (!res.transcript_path.empty()) {
        std::printf("wrote transcript %s (%zu segments, %zu flagged)\n",
                    res.transcript_path.c_str(), res.transcript.size(), res.flagged);
    }
    if (opts.transcribe) {
        std::printf("transcript: %zu segments, %zu containing PII\n",
                    res.transcript.size(), res.flagged);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// live
// ---------------------------------------------------------------------------
int cmd_live(Argv& a) {
    long in_dev = -1, out_dev = -1;
    long rate = 0;
    long frames = 1024;
    bool mute_silence = false;
    bool dry = false;
    dsp::ShiftParams shift;
    std::string preset;

    while (!a.done()) {
        const char* arg = a.peek();
        if (std::strcmp(arg, "--in") == 0) { a.next(); if (!opt_int(a, "--in", in_dev, false)) return 1; }
        else if (std::strcmp(arg, "--out") == 0) { a.next(); if (!opt_int(a, "--out", out_dev, false)) return 1; }
        else if (std::strcmp(arg, "--preset") == 0) { a.next(); if (!opt_string(a, "--preset", preset)) return 1; }
        else if (std::strcmp(arg, "--pitch") == 0) { a.next(); if (!opt_double(a, "--pitch", shift.pitch_semitones)) return 1; }
        else if (std::strcmp(arg, "--rate") == 0) { a.next(); if (!opt_int(a, "--rate", rate, false)) return 1; }
        else if (std::strcmp(arg, "--frames") == 0) { a.next(); if (!opt_int(a, "--frames", frames, true)) return 1; }
        else if (std::strcmp(arg, "--mute-silence") == 0) { a.next(); mute_silence = true; }
        else if (std::strcmp(arg, "--dry") == 0) { a.next(); dry = true; }
        else {
            std::fprintf(stderr, "error: unknown option '%s'\n", arg);
            return 1;
        }
    }

    if (shift.pitch_semitones < -24.0 || shift.pitch_semitones > 24.0) {
        std::fprintf(stderr, "error: --pitch must be within [-24, 24] semitones\n");
        return 1;
    }

    if (!audio::pa_init()) {
        std::fprintf(stderr, "error: PortAudio init failed\n");
        return 1;
    }

    if (!preset.empty()) {
        const dsp::Preset* p = dsp::preset_named(preset);
        if (!p) {
            std::fprintf(stderr, "error: unknown preset '%s'\n", preset.c_str());
            audio::pa_shutdown();
            return 1;
        }
        shift = p->params;
    }

    if (in_dev < 0) in_dev = audio::default_input_device();
    if (out_dev < 0) out_dev = audio::default_output_device();
    if (rate <= 0) rate = audio::default_sample_rate();
    if (in_dev < 0 || out_dev < 0) {
        std::fprintf(stderr, "error: no default input/output device\n");
        audio::pa_shutdown();
        return 1;
    }

    audio::LiveStream ls;
    std::string err;
    if (!audio::open_live(ls, in_dev, out_dev, static_cast<int>(rate),
                          static_cast<size_t>(frames), err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        audio::pa_shutdown();
        return 1;
    }

    const double factor = dsp::semitones_to_ratio(shift.pitch_semitones);
    std::unique_ptr<dsp::PitchShifter> shifter;
    try {
        shifter = std::make_unique<dsp::PitchShifter>(2048, 4,
                                                      dry ? 1.0 : factor);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        audio::close_live(ls);
        audio::pa_shutdown();
        return 1;
    }
    dsp::StreamVAD vad(static_cast<int>(rate));

    std::atomic<bool> stop{false};
    std::thread worker([&] {
        std::vector<float> in_buf(frames), out_buf(frames);
        std::vector<float> block_in, block_out;
        while (!stop.load()) {
            const size_t got = ls.input_rb.read(in_buf.data(), frames);
            if (got == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            block_in.assign(in_buf.begin(), in_buf.begin() + got);
            block_out.resize(got);

            const bool speech = vad.update(block_in.data(), got);
            if (!dry) {
                if (mute_silence && !speech) {
                    std::fill(block_out.begin(), block_out.end(), 0.0f);
                    ls.output_rb.write(block_out.data(), got);
                } else {
                    const size_t made =
                        shifter->process(block_in.data(), got, block_out.data(), got);
                    ls.output_rb.write(block_out.data(), made);
                }
            } else {
                block_out = block_in;
                ls.output_rb.write(block_out.data(), got);
            }
        }
    });

    if (!audio::start_live(ls)) {
        std::fprintf(stderr, "error: could not start stream\n");
        stop.store(true);
        worker.join();
        audio::close_live(ls);
        audio::pa_shutdown();
        return 1;
    }

    const auto in_devs = audio::list_devices();
    std::printf("live masking active (mic %ld -> %s, speaker %ld). pitch %.1f st."
                " Press Ctrl+C to stop.\n",
                in_dev,
                in_dev >= 0 && in_dev < static_cast<long>(in_devs.size())
                    ? in_devs[in_dev].name.c_str()
                    : "?",
                out_dev, shift.pitch_semitones);

    std::this_thread::sleep_for(std::chrono::hours(24));
    stop.store(true);
    audio::stop_live(ls);
    worker.join();
    audio::close_live(ls);
    audio::pa_shutdown();
    std::printf("stopped.\n");
    return 0;
}

// ---------------------------------------------------------------------------
// transcribe
// ---------------------------------------------------------------------------
int cmd_transcribe(Argv& a) {
    const std::string input = a.next();
    if (input.empty()) {
        std::fprintf(stderr, "error: transcribe requires an input file\n");
        return 1;
    }
    std::string output;
    bool redact = false;
    transcribe::WhisperConfig wcfg;

    while (!a.done()) {
        const char* arg = a.peek();
        if (std::strcmp(arg, "-o") == 0) { a.next(); if (!opt_string(a, "-o", output)) return 1; }
        else if (std::strcmp(arg, "--redact") == 0) { a.next(); redact = true; }
        else if (std::strcmp(arg, "--whisper") == 0) { a.next(); if (!opt_string(a, "--whisper", wcfg.binary)) return 1; }
        else if (std::strcmp(arg, "--model") == 0) { a.next(); if (!opt_string(a, "--model", wcfg.model)) return 1; }
        else if (std::strcmp(arg, "--lang") == 0) { a.next(); if (!opt_string(a, "--lang", wcfg.language)) return 1; }
        else {
            std::fprintf(stderr, "error: unknown option '%s'\n", arg);
            return 1;
        }
    }

    std::vector<Segment> segments;
    std::string err;
    if (!transcribe::transcribe(input, wcfg, segments, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    redact::RedactOptions ropts;
    if (redact) {
        redact::flag_segments(segments, ropts);
    }

    if (output.empty()) output = input + ".transcript.txt";
    std::ofstream f(output);
    if (!f) {
        std::fprintf(stderr, "error: cannot write %s\n", output.c_str());
        return 1;
    }
    for (const auto& seg : segments) {
        const int total = static_cast<int>(seg.start + 0.5);
        const int h = total / 3600, m = (total % 3600) / 60, s = total % 60;
        f << "[" << (h < 10 ? "0" : "") << h << ":" << (m < 10 ? "0" : "") << m
          << ":" << (s < 10 ? "0" : "") << s << "] " << seg.text << "\n";
    }
    std::printf("wrote %s (%zu segments)\n", output.c_str(), segments.size());
    return 0;
}

// ---------------------------------------------------------------------------
// anonymize (transcript file redaction)
// ---------------------------------------------------------------------------
int cmd_anonymize(Argv& a) {
    const std::string input = a.next();
    if (input.empty()) {
        std::fprintf(stderr, "error: anonymize requires a transcript file\n");
        return 1;
    }
    std::string output;
    bool synthetic = false;
    std::string replacement;

    while (!a.done()) {
        const char* arg = a.peek();
        if (std::strcmp(arg, "-o") == 0) { a.next(); if (!opt_string(a, "-o", output)) return 1; }
        else if (std::strcmp(arg, "--synthetic") == 0) { a.next(); synthetic = true; }
        else if (std::strcmp(arg, "--replacement") == 0) { a.next(); if (!opt_string(a, "--replacement", replacement)) return 1; }
        else {
            std::fprintf(stderr, "error: unknown option '%s'\n", arg);
            return 1;
        }
    }

    std::ifstream in(input);
    if (!in) {
        std::fprintf(stderr, "error: cannot open %s\n", input.c_str());
        return 1;
    }
    if (output.empty()) output = input + ".redacted.txt";
    std::ofstream out(output);
    if (!out) {
        std::fprintf(stderr, "error: cannot write %s\n", output.c_str());
        return 1;
    }

    redact::RedactOptions ropts;
    ropts.use_synthetic_names = synthetic;
    if (!replacement.empty()) ropts.replacement = replacement;

    std::string line;
    size_t flagged = 0;
    while (std::getline(in, line)) {
        // Preserve the "[HH:MM:SS] " timestamp prefix (both the short
        // `[00:00:00]` and the ranged `[00:00:00.000 - 00:00:05.000]` forms).
        std::string prefix;
        std::string body = line;
        if (line.size() >= 3 && line[0] == '[') {
            const size_t close = line.find(']');
            if (close != std::string::npos && close <= 48) {
                prefix = line.substr(0, close + 1);
                body = line.substr(close + 1);
                if (!body.empty() && body[0] == ' ') body = body.substr(1);
            }
        }
        std::vector<std::string> names;
        const std::string red = redact::redact_text(body, ropts, names);
        if (red != body) ++flagged;
        out << prefix << (prefix.empty() ? "" : " ") << red << "\n";
    }
    std::printf("wrote %s (%zu lines flagged)\n", output.c_str(), flagged);
    return 0;
}

// ---------------------------------------------------------------------------
// misc commands
// ---------------------------------------------------------------------------
int cmd_presets() {
    for (const auto& p : dsp::presets()) {
        std::printf("%-12s pitch %+5.1f st  formant %+5.1f st  %s\n",
                    p.name.c_str(), p.params.pitch_semitones,
                    p.params.formant_semitones, p.description.c_str());
    }
    return 0;
}

int cmd_devices() {
    if (!audio::pa_init()) {
        std::fprintf(stderr, "error: PortAudio init failed\n");
        return 1;
    }
    const auto devs = audio::list_devices();
    const int din = audio::default_input_device();
    const int dout = audio::default_output_device();
    for (const auto& d : devs) {
        std::printf("%3d  in=%d out=%d %5.0f Hz  %s%s%s\n", d.index,
                    d.max_inputs, d.max_outputs, d.default_sample_rate,
                    d.name.c_str(),
                    d.index == din ? "  [default in]" : "",
                    d.index == dout ? "  [default out]" : "");
    }
    audio::pa_shutdown();
    return 0;
}

// Implemented by the CLI target itself when the Qt GUI is compiled in
// (src/gui_launch.cpp); absent (weak == null) in GUI-less builds. This keeps
// the shared ocs_core library free of Qt dependencies.
extern "C" int ocs_gui_launch(int argc, char** argv) __attribute__((weak));

int cmd_gui() {
    if (ocs_gui_launch) return ocs_gui_launch(g_argc, g_argv);
    std::fprintf(stderr, "GUI support was not built. Reconfigure with Qt6 "
                         "available and OCS_GUI=ON.\n");
    return 1;
}

}  // namespace

int run(int argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
    // Apply AI connectivity settings from the environment
    // (OCS_OLLAMA_URL, OCS_LLAMA_URL, OCS_AI_LAUNCH_CMD, ...).
    llm::set_config(llm::config_from_env());
    if (argc < 2) {
        print_help();
        return 1;
    }
    Argv a{argc, argv};
    const std::string cmd = a.next();

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_help();
        return 0;
    }
    if (cmd == "info" || cmd == "--version" || cmd == "-v") {
        print_version();
        return 0;
    }
    if (cmd == "process") return cmd_process(a);
    if (cmd == "live") return cmd_live(a);
    if (cmd == "transcribe") return cmd_transcribe(a);
    if (cmd == "anonymize") return cmd_anonymize(a);
    if (cmd == "presets") return cmd_presets();
    if (cmd == "devices") return cmd_devices();
    if (cmd == "gui") return cmd_gui();

    std::fprintf(stderr, "unknown command '%s'\n\n", cmd.c_str());
    print_help();
    return 1;
}

}  // namespace ocs::cli

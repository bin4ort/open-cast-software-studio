// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "dsp/compare.h"

#include <algorithm>
#include <cmath>

namespace ocs::dsp {

namespace {

// RMS of data[begin, end), 0 for an empty range.
double rms(const std::vector<float>& data, size_t begin, size_t end) {
    if (end <= begin || begin >= data.size()) return 0.0;
    end = std::min(end, data.size());
    double sum = 0.0;
    for (size_t i = begin; i < end; ++i) sum += data[i] * data[i];
    return std::sqrt(sum / static_cast<double>(end - begin));
}

// Builds a normalized (0..1) RMS envelope split into `buckets` chunks.
std::vector<double> envelope(const std::vector<float>& data, size_t buckets) {
    std::vector<double> env(buckets, 0.0);
    if (data.empty() || buckets == 0) return env;
    double peak = 0.0;
    const size_t n = data.size();
    for (size_t b = 0; b < buckets; ++b) {
        const size_t begin = b * n / buckets;
        const size_t end = (b + 1) * n / buckets;
        env[b] = rms(data, begin, end);
        peak = std::max(peak, env[b]);
    }
    if (peak > 0.0) {
        for (auto& v : env) v /= peak;
    }
    return env;
}

}  // namespace

AudioDiff analyze_diff(const std::vector<float>& orig,
                       const std::vector<float>& proc, size_t buckets) {
    AudioDiff d;
    d.orig_len = orig.size();
    d.proc_len = proc.size();
    d.buckets = buckets;
    if (buckets == 0) return d;
    if (orig.empty() || proc.empty()) return d;

    d.orig_env = envelope(orig, buckets);
    d.proc_env = envelope(proc, buckets);

    d.orig_rms = rms(orig, 0, orig.size());
    d.proc_rms = rms(proc, 0, proc.size());
    if (d.orig_rms > 1e-12) d.rms_ratio = d.proc_rms / d.orig_rms;

    d.peak_orig = 0.0;
    for (float v : orig) d.peak_orig = std::max(d.peak_orig, static_cast<double>(std::fabs(v)));
    d.peak_proc = 0.0;
    for (float v : proc) d.peak_proc = std::max(d.peak_proc, static_cast<double>(std::fabs(v)));

    const size_t aligned = std::min(orig.size(), proc.size());
    d.aligned_len = aligned;
    if (aligned == 0) return d;

    double sum = 0.0;
    for (size_t i = 0; i < aligned; ++i) {
        sum += std::fabs(orig[i] - proc[i]);
    }
    d.mean_abs_dev = sum / static_cast<double>(aligned);
    return d;
}

}  // namespace ocs::dsp

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

namespace ocs::audio {

// Single-producer single-consumer lock-free ring buffer of float samples.
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);

    size_t write(const float* data, size_t n);
    size_t read(float* data, size_t n);
    size_t available() const;
    size_t space() const;
    void reset();

private:
    size_t mask_;
    std::vector<float> buf_;
    std::atomic<size_t> head_{0};  // next write index
    std::atomic<size_t> tail_{0};  // next read index
};

}  // namespace ocs::audio

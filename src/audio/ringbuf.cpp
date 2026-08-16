// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "audio/ringbuf.h"

#include <algorithm>
#include <cstddef>

namespace ocs::audio {

RingBuffer::RingBuffer(size_t capacity) {
    size_t cap = 1;
    while (cap < capacity) cap <<= 1;
    mask_ = cap - 1;
    buf_.resize(cap);
}

size_t RingBuffer::write(const float* data, size_t n) {
    size_t written = 0;
    const size_t cap = mask_ + 1;
    while (written < n) {
        // head/tail are monotonic 64-bit counters; `used` is never masked,
        // so an exactly-full ring (head == tail after a full wrap) cannot be
        // mistaken for empty. Only the buffer index is masked.
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_acquire);
        const size_t used = head - tail;
        if (used == cap) break;  // full
        const size_t space = cap - used;
        const size_t chunk = std::min(n - written, space);
        for (size_t i = 0; i < chunk; ++i) {
            buf_[(head + i) & mask_] = data[written + i];
        }
        head_.store(head + chunk, std::memory_order_release);
        written += chunk;
    }
    return written;
}

size_t RingBuffer::read(float* data, size_t n) {
    size_t got = 0;
    while (got < n) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t used = head - tail;
        if (used == 0) break;  // empty
        const size_t chunk = std::min(n - got, used);
        for (size_t i = 0; i < chunk; ++i) {
            data[got + i] = buf_[(tail + i) & mask_];
        }
        tail_.store(tail + chunk, std::memory_order_release);
        got += chunk;
    }
    return got;
}

size_t RingBuffer::available() const {
    const size_t head = head_.load(std::memory_order_acquire);
    const size_t tail = tail_.load(std::memory_order_acquire);
    return head - tail;
}

size_t RingBuffer::space() const {
    return mask_ + 1 - available();
}

void RingBuffer::reset() {
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
}

}  // namespace ocs::audio

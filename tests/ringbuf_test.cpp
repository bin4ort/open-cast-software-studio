// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "test_util.h"

#include <vector>

#include "audio/ringbuf.h"

using ocs::audio::RingBuffer;

TEST(ringbuf_round_trip) {
    RingBuffer rb(16);
    std::vector<float> in = {1, 2, 3, 4, 5};
    std::vector<float> out(5, 0.0f);
    CHECK(rb.write(in.data(), in.size()) == in.size());
    CHECK(rb.read(out.data(), out.size()) == out.size());
    for (size_t i = 0; i < in.size(); ++i) CHECK_NEAR(out[i], in[i], 1e-6f);
}

TEST(ringbuf_wraparound) {
    RingBuffer rb(16);
    std::vector<float> in(12, 1.0f);
    std::vector<float> out(16, 0.0f);
    CHECK(rb.write(in.data(), 12) == 12);
    CHECK(rb.read(out.data(), 8) == 8);  // 4 left; head now past capacity end
    CHECK(rb.write(in.data(), 12) == 12);  // wraps around the mask
    CHECK(rb.available() == 16);
    CHECK(rb.read(out.data(), 16) == 16);
    for (float v : out) CHECK_NEAR(v, 1.0f, 1e-6f);
    CHECK(rb.available() == 0);
}

TEST(ringbuf_full_drops) {
    RingBuffer rb(8);  // capacity rounds up to 8
    std::vector<float> in(16, 1.0f);
    const size_t written = rb.write(in.data(), 16);
    CHECK(written == 8);  // full: rest dropped, no overrun
    std::vector<float> out(16, 0.0f);
    CHECK(rb.read(out.data(), 16) == 8);
    for (size_t i = 0; i < 8; ++i) CHECK_NEAR(out[i], 1.0f, 1e-6f);
}

TEST(ringbuf_empty_read) {
    RingBuffer rb(8);
    std::vector<float> out(4, 1.0f);
    CHECK(rb.read(out.data(), 4) == 0);
    CHECK_NEAR(out[0], 1.0f, 1e-6f);  // untouched on underflow
}

TEST(ringbuf_capacity_rounds_up) {
    RingBuffer rb(3);
    std::vector<float> in(4, 1.0f);
    CHECK(rb.write(in.data(), 4) == 4);  // capacity is 4, not 3
    CHECK(rb.available() == 4);
}

TEST(ringbuf_partial_chunk_reads) {
    RingBuffer rb(16);
    std::vector<float> in(16, 2.0f);
    CHECK(rb.write(in.data(), 16) == 16);
    std::vector<float> out(16, 0.0f);
    size_t total = 0;
    for (size_t chunk : {3u, 5u, 8u}) {
        total += rb.read(out.data() + total, chunk);
    }
    CHECK(total == 16);
    for (float v : out) CHECK_NEAR(v, 2.0f, 1e-6f);
}

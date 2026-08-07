// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ui/waveform.h"

#include <QPainter>
#include <QPen>

#include <algorithm>

namespace ocs::ui {

Waveform::Waveform(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(140);
    setMinimumWidth(300);
    setAutoFillBackground(true);
}

QSize Waveform::minimumSizeHint() const {
    return QSize(300, 140);
}

void Waveform::set_samples(const std::vector<float>& samples, int sample_rate) {
    samples_ = samples;
    sample_rate_ = sample_rate;
    update();
}

void Waveform::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), palette().base());
    p.setPen(QPen(palette().mid(), 1));

    const int w = width();
    const int h = height();
    const int mid = h / 2;

    // Horizontal center line.
    p.drawLine(0, mid, w, mid);

    if (samples_.empty()) {
        p.drawText(rect(), Qt::AlignCenter, "No audio loaded");
        return;
    }

    const size_t n = samples_.size();
    const float scale = 0.9f * mid;

    p.setPen(QPen(palette().text(), 1));

    const size_t stride = n > static_cast<size_t>(w) ? n / static_cast<size_t>(w) : 1;
    int prev_x = -1;
    int prev_y = mid;
    for (int x = 0; x < w && static_cast<size_t>(x) * stride < n; ++x) {
        const float s = samples_[static_cast<size_t>(x) * stride];
        float v = s * scale;
        if (v > mid) v = static_cast<float>(mid);
        if (v < -mid) v = static_cast<float>(-mid);
        const int y = mid - static_cast<int>(v);
        if (prev_x >= 0) {
            p.drawLine(prev_x, prev_y, x, y);
        }
        prev_x = x;
        prev_y = y;
    }
}

}  // namespace ocs::ui

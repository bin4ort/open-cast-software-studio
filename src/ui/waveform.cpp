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

void Waveform::set_overlay(const std::vector<float>& samples) {
    overlay_ = samples;
    update();
}

void Waveform::set_compare_mode(bool on) {
    compare_ = on;
    update();
}

void Waveform::draw_curve(QPainter& p, const std::vector<float>& data, int w,
                          int mid, float scale, const QColor& color,
                          int pen_width) {
    const size_t n = data.size();
    if (n == 0) return;
    const size_t stride = n > static_cast<size_t>(w) ? n / static_cast<size_t>(w) : 1;
    QPen pen(color, pen_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    int prev_x = -1;
    int prev_y = mid;
    for (int x = 0; x < w && static_cast<size_t>(x) * stride < n; ++x) {
        const float s = data[static_cast<size_t>(x) * stride];
        float v = s * scale;
        v = std::clamp(v, -static_cast<float>(mid), static_cast<float>(mid));
        const int y = mid - static_cast<int>(v);
        if (prev_x >= 0) p.drawLine(prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }
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

    const float scale = 0.9f * mid;
    if (compare_ && !overlay_.empty()) {
        // Grayscale overlay (the other recording) drawn first, behind the
        // vivid main waveform.
        draw_curve(p, overlay_, w, mid, scale, QColor(150, 155, 165, 200), 2);
        draw_curve(p, samples_, w, mid, scale, palette().highlight().color(), 2);
    } else {
        draw_curve(p, samples_, w, mid, scale, palette().text().color(), 1);
    }
}

}  // namespace ocs::ui

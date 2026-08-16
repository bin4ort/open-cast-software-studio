// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <QWidget>

#include <cstddef>
#include <vector>

namespace ocs::ui {

// Waveform renderer: draws a mono PCM buffer as a polyline. Supports a
// comparison mode where a second buffer (e.g. the processed output) is drawn
// as a grayscale overlay behind the vivid main waveform, so differences are
// easy to spot. Both buffers are time-aligned proportionally and normalized
// to the same amplitude scale.
class Waveform : public QWidget {
    Q_OBJECT

public:
    explicit Waveform(QWidget* parent = nullptr);

    void set_samples(const std::vector<float>& samples, int sample_rate);
    void set_sample_rate(int sample_rate) { sample_rate_ = sample_rate; }

    // Comparison overlay (drawn grayscale behind the main waveform).
    void set_overlay(const std::vector<float>& samples);
    void set_compare_mode(bool on);
    bool compare_mode() const { return compare_; }

    // Access for the compare/AI features (e.g. feeding the curves to an LLM).
    const std::vector<float>& samples() const { return samples_; }
    const std::vector<float>& overlay() const { return overlay_; }

    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void draw_curve(QPainter& p, const std::vector<float>& data, int w, int mid,
                    float scale, const QColor& color, int pen_width);

    std::vector<float> samples_;
    std::vector<float> overlay_;
    bool compare_ = false;
    int sample_rate_ = 48000;
};

}  // namespace ocs::ui

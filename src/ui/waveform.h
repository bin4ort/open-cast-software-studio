// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#pragma once

#include <QWidget>

#include <cstddef>
#include <vector>

namespace va::ui {

// Simple waveform renderer: draws a mono PCM buffer as a filled polyline.
class Waveform : public QWidget {
    Q_OBJECT

public:
    explicit Waveform(QWidget* parent = nullptr);

    void set_samples(const std::vector<float>& samples, int sample_rate);
    void set_sample_rate(int sample_rate) { sample_rate_ = sample_rate; }

    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<float> samples_;
    int sample_rate_ = 48000;
};

}  // namespace va::ui

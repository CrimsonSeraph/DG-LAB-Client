#include "DebugLog.h"
#include "SampledWaveformWidget.h"

#include <QPainter>
#include <QPen>

#include <algorithm>

SampledWaveformWidget::SampledWaveformWidget(QWidget* parent)
    : QWidget(parent)
    , sample_index_(0)
    , latest_input_value_(0.0)
    , has_new_input_(false)
    , sample_interval_ms_(kDefaultSampleIntervalMs)
    , max_amplitude_(kDefaultMaxAmplitude) {
    // 初始化采样缓冲区，全部填0
    samples_.fill(0.0, kMaxSamplePoints);

    // 启动采样定时器
    sample_timer_.setInterval(sample_interval_ms_);
    connect(&sample_timer_, &QTimer::timeout, this, &SampledWaveformWidget::on_sample_timer);
    sample_timer_.start();

    // 设置黑色背景
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

    LOG_MODULE("SampledWaveformWidget", "set_max_amplitude", LOG_DEBUG,
        "初始化完成，采样间隔=" << sample_interval_ms_ << "ms，最大振幅比例=" << max_amplitude_);
}

void SampledWaveformWidget::input_data(double value) {
    QMutexLocker locker(&mutex_);
    value = qBound(0.0, value, 1.0);
    latest_input_value_ = value;
    has_new_input_ = true;
}

void SampledWaveformWidget::set_sample_interval_ms(int ms) {
    if (ms <= 0) {
        LOG_MODULE("SampledWaveformWidget", "set_max_amplitude", LOG_WARN,
            "无效采样间隔=" << ms << "，保持原值=" << sample_interval_ms_);
        return;
    }
    sample_interval_ms_ = ms;
    sample_timer_.setInterval(ms);
    LOG_MODULE("SampledWaveformWidget", "set_max_amplitude", LOG_DEBUG,
        "采样间隔已更新为 " << ms << "ms");
}

void SampledWaveformWidget::set_max_amplitude(double ratio) {
    max_amplitude_ = qBound(0.1, ratio, 1.0);
    LOG_MODULE("SampledWaveformWidget", "set_max_amplitude", LOG_DEBUG,
        "最大振幅比例已更新为 " << max_amplitude_);
    update();
}

void SampledWaveformWidget::on_sample_timer() {
    double value_to_sample = 0.0;
    {
        QMutexLocker locker(&mutex_);
        if (has_new_input_) {
            value_to_sample = latest_input_value_;
            has_new_input_ = false;
        }
        else {
            value_to_sample = 0.0;
        }
    }
    add_sample(value_to_sample);
    update();
}

void SampledWaveformWidget::add_sample(double value) {
    samples_[sample_index_] = value;
    sample_index_ = (sample_index_ + 1) % kMaxSamplePoints;
}

void SampledWaveformWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    const int baseline_y = h - 5;   // 底部边距5像素
    const int top_margin = 5;
    const int left_margin = 0;
    const int right_margin = 0;

    // 灰色虚线基线
    painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
    painter.drawLine(left_margin, baseline_y, w - right_margin, baseline_y);

    // 获取有序采样点（从旧到新）
    QVector<double> ordered_samples;
    ordered_samples.reserve(kMaxSamplePoints);
    for (int i = 0; i < kMaxSamplePoints; ++i) {
        int idx = (sample_index_ + i) % kMaxSamplePoints;
        ordered_samples.append(samples_[idx]);
    }

    // 构建折线点集
    QVector<QPointF> points;
    points.reserve(kMaxSamplePoints);
    double step = static_cast<double>(w - left_margin - right_margin) / (kMaxSamplePoints - 1);
    double amplitude_range = baseline_y - top_margin;

    for (int i = 0; i < kMaxSamplePoints; ++i) {
        double x = left_margin + i * step;
        double y = baseline_y - amplitude_range * max_amplitude_ * ordered_samples[i];
        y = qBound(static_cast<double>(top_margin), y, static_cast<double>(baseline_y));
        points.append(QPointF(x, y));
    }

    // 绘制绿色波形
    painter.setPen(QPen(Qt::green, 2));
    for (int i = 0; i < points.size() - 1; ++i) {
        painter.drawLine(points[i], points[i + 1]);
    }
}

void SampledWaveformWidget::resizeEvent(QResizeEvent* event) {
    update();
}

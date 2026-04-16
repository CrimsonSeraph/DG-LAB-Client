#pragma once

#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QMutex>

class SampledWaveformWidget : public QWidget {
    Q_OBJECT
        Q_PROPERTY(int sample_interval_ms READ sample_interval_ms WRITE set_sample_interval_ms)
        Q_PROPERTY(double max_amplitude READ max_amplitude WRITE set_max_amplitude)

public:
    explicit SampledWaveformWidget(QWidget* parent = nullptr);
    ~SampledWaveformWidget() = default;

    /// @brief 输入一个新数据（归一化值 0.0 ~ 1.0）
    /// @note 此值会被采样线程在下一次采样时使用，如果多次调用只保留最后一次
    void input_data(double value);

    int sample_interval_ms() const { return sample_interval_ms_; }
    void set_sample_interval_ms(int ms);

    double max_amplitude() const { return max_amplitude_; }
    void set_max_amplitude(double ratio);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void on_sample_timer(); // 定时采样

private:
    // 常量
    static constexpr int kDefaultSampleIntervalMs = 20; // 采样间隔 20ms
    static constexpr double kDefaultMaxAmplitude = 0.8;
    static constexpr int kMaxSamplePoints = 200;    // 最多保留200个采样点（4秒波形）

    // 成员变量
    QTimer sample_timer_;   // 采样定时器
    QVector<double> samples_;   // 采样值环形缓冲区（0~1）
    qint64 sample_index_;   // 当前要写入的索引（环形）
    double latest_input_value_; // 最新的输入值（待采样）
    bool has_new_input_;    // 是否有新输入未采样
    mutable QMutex mutex_;  // 保护 latest_input_value_ 和 has_new_input_

    int sample_interval_ms_;    // 采样间隔（毫秒）
    double max_amplitude_;  // 最大振幅比例

    void add_sample(double value);  // 添加一个采样点到缓冲区
};

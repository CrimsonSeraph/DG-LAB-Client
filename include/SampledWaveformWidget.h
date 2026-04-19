/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QColor>
#include <QMutex>
#include <QTimer>
#include <QWidget>
#include <QVector>

#include <map>
#include <string>
#include <vector>

class SampledWaveformWidget : public QWidget {
    Q_OBJECT
        Q_PROPERTY(int sample_interval_ms READ sample_interval_ms WRITE set_sample_interval_ms)
        Q_PROPERTY(double max_amplitude READ max_amplitude WRITE set_max_amplitude)
        Q_PROPERTY(int max_listeners READ max_listeners WRITE set_max_listeners)

public:
    // -------------------- 构造/析构 --------------------
    /*
     * @brief 构造函数
     * @param parent 父窗口指针，默认为 nullptr
     */
    explicit SampledWaveformWidget(QWidget* parent = nullptr);
    ~SampledWaveformWidget() = default;

    // -------------------- 监听器管理 --------------------
    /*
     * @brief 添加一个监听器
     * @param name 监听器名称（唯一标识）
     * @param color 波形颜色
     * @param min 输入值的原始最小值（包含）
     * @param max 输入值的原始最大值（包含）
     * @return true 添加成功，false 失败（名称已存在或已达上限）
     */
    bool add_listener(const std::string& name, const QColor& color, double min, double max);
    bool add_listener(const std::string& name, const QColor& color);    ///> 不设置输入范围的重载版本（默认 0.0~1.0）

    /*
     * @brief 移除监听器
     * @param name 监听器名称
     * @return true 移除成功，false 监听器不存在或为默认监听器
     */
    bool remove_listener(const std::string& name);

    /*
     * @brief 修改监听器颜色
     * @param name 监听器名称
     * @param color 新颜色
     * @return true 修改成功，false 监听器不存在
     */
    bool set_listener_color(const std::string& name, const QColor& color);

    /*
     * @brief 获取所有监听器名称列表
     * @return 监听器名称的 vector
     */
    std::vector<std::string> get_listener_names() const;

    /*
     * @brief 检查监听器是否存在
     * @param name 监听器名称
     * @return true 存在，false 不存在
     */
    bool has_listener(const std::string& name) const;

    /*
     * @brief 设置最大监听器数量上限
     * @param limit 上限值（必须 >= 1）
     */
    void set_max_listeners(int limit);

    /*
     * @brief 获取最大监听器数量上限
     * @return 上限值
     */
    inline int max_listeners() const { return max_listeners_; }

    // -------------------- 数据输入 --------------------
    /*
     * @brief 为指定监听器输入新数据（归一化值 0.0 ~ 1.0）
     * @param listener_name 监听器名称
     * @param value 输入值（自动裁剪到 [0,1]）
     * @note 如果监听器不存在，会尝试创建默认监听器（名称为 "default"），否则忽略
     */
    void input_data(const std::string& listener_name, double value);

    /*
     * @brief 为默认监听器输入数据（兼容旧接口）
     * @param value 输入值
     */
    inline void input_data(double value) { input_data("default", value); }

    /*
     * @brief 设置监听器的原始输入范围（浮点数版本）
     * @param name 监听器名称
     * @param min 原始最小值（包含）
     * @param max 原始最大值（包含）
     * @note 后续调用 input_data() 时，原始值会被线性映射到 [0,1] 范围。
     *       若 min >= max，设置无效并输出警告。
     */
    void set_input_range(const std::string& name, double min, double max);

    /*
     * @brief 设置监听器的原始输入范围（整数版本）
     * @param name 监听器名称
     * @param min 原始最小值（包含）
     * @param max 原始最大值（包含）
     */
    void set_input_range(const std::string& name, int min, int max);

    // -------------------- 全局配置 --------------------
    /*
     * @brief 获取采样间隔
     * @return 采样间隔（毫秒）
     */
    inline int sample_interval_ms() const { return sample_interval_ms_; }


    /*
     * @brief 获取最大振幅比例
     * @return 比例值（0.1 ~ 1.0）
     */
    inline double max_amplitude() const { return max_amplitude_; }

    /*
     * @brief 设置采样间隔
     * @param ms 采样间隔（毫秒），必须 > 0
     */
    void set_sample_interval_ms(int ms);

    /*
     * @brief 设置最大振幅比例
     * @param ratio 比例值（自动裁剪到 [0.1, 1.0]）
     */
    void set_max_amplitude(double ratio);

protected:
    // -------------------- Qt 事件重写 --------------------
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    // -------------------- 定时器槽函数 --------------------
    void on_sample_timer();

private:
    // -------------------- 常量 --------------------
    static constexpr int kDefaultSampleIntervalMs = 20; ///> 默认采样间隔 20ms
    static constexpr double kDefaultMaxAmplitude = 0.8; ///> 默认最大振幅比例
    static constexpr int kMaxSamplePoints = 200;    ///> 每个监听器最多保留200个采样点
    static constexpr int kDefaultMaxListeners = 16; ///> 默认最大监听器数量

    // -------------------- 内部数据结构 --------------------
    /*
     * @brief 单个监听器的内部数据结构
     */
    struct ListenerData {
        QVector<double> samples;    ///> 采样值环形缓冲区（0~1）
        qint64 sample_index;    ///> 当前要写入的索引
        double latest_value;    ///> 最新的输入值（待采样）
        bool has_new_input; ///> 是否有新输入未采样
        QColor color;   ///> 波形颜色
        double input_min;   ///> 原始输入最小值（线性映射到0）
        double input_max;   ///> 原始输入最大值（线性映射到1）

         // 默认构造函数（范围 [0,1]）
        inline ListenerData(const QColor& col = Qt::green)
            : sample_index(0), latest_value(0.0), has_new_input(false), color(col),
            input_min(0.0), input_max(1.0) {
            samples.fill(0.0, kMaxSamplePoints);
        }

        // 带范围的构造函数
        inline ListenerData(const QColor& col, double min, double max)
            : sample_index(0), latest_value(0.0), has_new_input(false), color(col),
            input_min(min), input_max(max) {
            samples.fill(0.0, kMaxSamplePoints);
        }
    };

    // -------------------- 成员变量 --------------------
    QTimer sample_timer_;   ///> 采样定时器
    std::map<std::string, ListenerData> listeners_; ///> 监听器映射（名称 -> 数据）
    mutable QMutex mutex_;  ///> 并发访问保护
    int sample_interval_ms_;    ///> 采样间隔（毫秒）
    double max_amplitude_;  ///> 最大振幅比例（全局）
    int max_listeners_; ///> 最大监听器数量上限

    // -------------------- 私有辅助函数 --------------------
    /*
     * @brief 为指定监听器添加一个采样点
     * @param name 监听器名称（必须存在）
     * @param value 采样值（0~1）
     */
    void add_sample(const std::string& name, double value);

    /*
     * @brief 确保默认监听器存在（名称为 "default"，颜色绿色）
     */
    void ensure_default_listener();
};

/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "EventDefinition.h"
#include "EventMath.h"
#include "EventMapping.h"

#include <QString>
#include <QStringList>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// 事件引擎：触发判定、每帧强度合成与触发流水；
// 不依赖 QObject 与定时器，时间由外部传入，波形帧按调用次数推进

/// @brief 触发流水条目
struct TriggerRecord {
    std::int64_t timestamp_ms = 0; ///< 触发时刻（毫秒，取最近一次 advance_frame 的时间）
    std::string module_name;       ///< 模块标识
    std::string event_id;          ///< 事件标识
    std::string event_label;       ///< 事件展示名
    std::string option_id;         ///< 子选项标识
    std::string option_label;      ///< 子选项展示名（无子选项时为空）
    std::string channel;           ///< 输出通道（"A"/"B"）
    std::string operation;         ///< 操作文本
    int delta = 0;                 ///< 本次输出请求引起的强度变化量
};

// EventEngine - 事件映射引擎
// 载入事件声明与用户映射后，按外部传入的时间逐帧合成 A/B 通道强度并记录触发流水；
// 事件触发后按 events.<事件>.frames 维持激活窗口，到期把该事件的通道累计状态整体归零
class EventEngine {
public:
    EventEngine() = default;

    /// @brief 载入模块的事件声明与映射（重复载入重建该模块的运行期状态）
    /// @param module_name 模块标识
    /// @param definitions 事件声明列表
    /// @param configs 事件映射表（键为事件 id）
    /// @note 载入时丢弃该模块已观察到的前值，下次写入按首次处理
    void configure(const std::string& module_name, const std::vector<EventDefinition>& definitions,
        const std::map<std::string, EventConfig>& configs);

    /// @brief 复位运行期状态（累加器、前值、通道强度与帧计数）
    /// @note 已载入的声明与映射、触发流水保留
    void clear();

    /// @brief 数值变化入口：按各事件触发条件登记激活并逐个产出输出请求
    /// @param module_name 模块标识
    /// @param value_id 数值 ID
    /// @param value 最新数值
    /// @note 该 (模块, 数值) 首次写入没有前值，不判定任何比较方式
    /// @note 激活窗口内重复触发只重置窗口，不清零累加器
    void on_value_changed(const std::string& module_name, const std::string& value_id, int value);

    /// @brief 推进一帧（固定 100ms）并刷新通道强度
    /// @param now_ms 当前时间戳（毫秒，由外部时钟提供）
    void advance_frame(std::int64_t now_ms);

    /// @brief 最近一帧的通道强度
    /// @param channel 通道（"A"/"B"）
    /// @return 强度整数，通道非法或未配置返回 0
    int channel_output(const std::string& channel) const;

    /// @brief 触发流水
    /// @param limit 最多返回条数
    /// @return 最近 limit 条记录（最新在后），limit 不大于 0 时为空
    std::vector<TriggerRecord> recent_triggers(int limit) const;

private:
    /// @brief 单个 ramp 激活的运行期状态
    struct RampState {
        double step = 0.0;     ///< 每帧步进（带符号）
        double target = 0.0;   ///< 目标值（带符号）
        double current = 0.0;  ///< 当前累加值
        int frames = 0;        ///< 已推进帧数
    };

    /// @brief 事件在单个通道上的累计状态
    struct ChannelState {
        double base = 0.0;             ///< set 基底
        double increase_sum = 0.0;     ///< increase 增量之和
        double decrease_sum = 0.0;     ///< decrease 减量之和
        std::vector<RampState> ramps;  ///< 活跃 ramp
    };

    /// @brief 子选项运行期状态
    struct RuntimeOption {
        std::string id;       ///< 子选项标识
        std::string label;    ///< 子选项展示名
        OptionOutput output;  ///< 输出映射
    };

    /// @brief 事件运行期状态
    struct RuntimeEvent {
        std::string id;                                         ///< 事件标识
        std::string label;                                      ///< 事件展示名
        EventTrigger trigger;                                   ///< 触发条件
        bool enabled = false;                                   ///< 是否参与输出
        MergeRule merge = MergeRule::MULTIPLY;                  ///< 合并规则
        double weight = 1.0;                                    ///< 合并权重
        std::vector<RuntimeOption> options;                     ///< 子选项（无声明时为隐式 default）
        bool on_channel[2] = {false, false};                    ///< 是否在该通道上有子选项（0=A，1=B）
        std::optional<int> min_value;                           ///< 触发数值下限（未注册时为空）
        std::optional<int> max_value;                           ///< 触发数值上限（未注册时为空）
        QStringList frames;                                     ///< 事件波形帧
        bool triggered = false;                                 ///< 是否已被触发
        int window_frames = DEFAULT_EVENT_FRAMES;               ///< 激活窗口帧数
        int remaining_frames = 0;                               ///< 剩余激活帧数
        ChannelState channels[2];                               ///< 各通道累计状态（0=A，1=B）
    };

    /// @brief 模块运行期状态
    struct ModuleState {
        std::string name;                                     ///< 模块标识
        std::string default_channel = DEFAULT_MODULE_CHANNEL; ///< 默认输出通道（"A"/"B"/"AB"）
        std::map<std::string, RuntimeEvent> events;           ///< 事件运行期状态（键为事件 id）
        int limits[2] = {200, 200};                           ///< 通道上限（0=A，1=B）
        int safeties[2] = {150, 150};                         ///< 通道安全上限（0=A，1=B）
        QStringList output_frames;                            ///< 输出端附加波形帧
        MergeRule output_merge = MergeRule::MULTIPLY;         ///< 输出端合并规则
    };

    /// @brief 单个 (模块, 数值) 的上一次值
    struct LastValue {
        int value = 0;       ///< 上一次数值
        bool valid = false;  ///< 是否已写入过
    };

    static constexpr int TRIGGER_CAPACITY = 200; ///< 触发流水容量
    static constexpr int RAMP_MAX_FRAMES = 100; ///< 单个 ramp 的帧数保护上限

    // 成员变量
    std::map<std::string, ModuleState> modules_;                          ///< 模块运行期状态（键为模块名）
    std::map<std::pair<std::string, std::string>, LastValue> last_values_; ///< 上一次数值（键为模块 + 数值）
    std::deque<TriggerRecord> triggers_;                                  ///< 触发流水（最新在后）
    std::map<std::string, int> channel_outputs_;                          ///< 最近一帧的通道强度
    std::int64_t last_time_ms_ = 0;                                       ///< 最近一次帧时间
    std::int64_t frame_index_ = 0;                                        ///< 帧计数（波形帧推进）

    // 私有辅助函数
    /// @brief 解析子选项实际生效的通道
    /// @param option 子选项（channel 为空串时跟随模块默认通道）
    /// @param module 模块运行期状态
    /// @param indices 生效通道索引（0=A，1=B）
    /// @return 生效通道个数（0~2，AB 时两个）
    int option_channels(const RuntimeOption& option, const ModuleState& module, int indices[2]) const;
    /// @brief 子选项按映射模式折算为强度值
    /// @param event 事件运行期状态
    /// @param output 输出映射
    /// @param limit 通道上限
    /// @return 折算后的强度值
    double option_mode_value(const RuntimeEvent& event, const OptionOutput& output, int limit) const;
    /// @brief 应用单个输出请求并登记流水
    /// @param module 模块运行期状态
    /// @param event 事件运行期状态
    /// @param option 子选项
    /// @param channel 目标通道（"A"/"B"）
    void apply_option(ModuleState& module, RuntimeEvent& event, const RuntimeOption& option,
        const std::string& channel);
    /// @brief 推进事件在某通道上的 ramp 累加器（到达目标或超时即结束并归零）
    /// @param state 通道累计状态
    /// @return 本帧 ramp 累加值之和
    double advance_ramps(ChannelState& state) const;
    /// @brief 递减各已触发事件的激活窗口（ramp 活跃时本帧不递减；到期把该事件两个通道的累计状态整体归零）
    void expire_events();
    /// @brief 波形当前帧的包络
    /// @param frames 波形帧
    /// @return 0~1 的包络，未绑定波形时返回 1
    double envelope_of(const QStringList& frames) const;
    /// @brief 按事件自身规则折叠两个归一值
    /// @param accumulator 当前累积比例（0~1）
    /// @param value 当前事件的归一值（0~1）
    /// @param rule 当前事件的合并规则
    /// @param weight 当前事件的合并权重
    /// @return 折叠后的比例
    double fold_merge(double accumulator, double value, MergeRule rule, double weight) const;
    /// @brief 追加流水（超出容量丢弃最旧）
    /// @param record 流水条目
    void push_trigger(const TriggerRecord& record);
};

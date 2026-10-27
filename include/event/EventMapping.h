/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "EventDefinition.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

// 子选项操作类型（ramp_* 仅当 mode=value 时有效）
enum class EventOperation {
    INCREASE = 0, ///< 增加
    SET = 1,      ///< 设为（同通道互斥）
    DECREASE = 2, ///< 减少
    RAMP_UP = 3,  ///< 逐步增加
    RAMP_DOWN = 4 ///< 逐步减少
};

// 子选项数值模式
enum class OptionMode {
    VALUE = 0,  ///< 绝对值
    PERCENT = 1 ///< 百分比（按通道上限折算）
};

// 波形合并规则
enum class MergeRule {
    MULTIPLY = 0, ///< 逐帧相乘
    MAX = 1,      ///< 取较大值
    MIN = 2,      ///< 取较小值
    WEIGHTED = 3  ///< 按权重加权
};

// 模块级默认输出通道（app.plugins.<模块>.channel）
constexpr const char* DEFAULT_MODULE_CHANNEL = "AB"; ///< 缺省值：A 与 B 各一次

/// @brief 通道配置（app.channels.<通道>，全局唯一，与插件无关）
/// @note 旧键 app.plugins.<模块>.channels.<通道> 在全局键缺失时被迁移一次
struct EventChannelConfig {
    int limit = 200;  ///< 通道上限
    int safety = 150; ///< 通道安全上限
};

/// @brief 子选项输出映射（events.<事件>.options.<子选项>）
struct OptionOutput {
    std::string channel = "A";                           ///< 目标通道（"A"/"B"，空串表示跟随模块默认通道）
    EventOperation operation = EventOperation::INCREASE; ///< 操作
    OptionMode mode = OptionMode::VALUE;                 ///< 数值模式
    int value = 0;                                       ///< 绝对值（mode=value 时使用）
    int percent = 0;                                     ///< 百分比（mode=percent 时使用）
    int step = 1;                                        ///< 步进（ramp_* 使用）
    std::int64_t edited_at = 0;                          ///< 最后编辑时间（毫秒时间戳）
};

// 事件输出数据类型（events.<事件>.dataType）：决定触发数值如何折算为输出
enum class EventDataType {
    STRENGTH = 0, ///< 强度：数值直接作为强度值
    PERCENT = 1,  ///< 百分比：数值按触发数值的上下限映射到 0~100
    WAVE = 2      ///< 波形：数值绑定波形帧序列参与叠加
};

// 事件激活窗口（1 帧 = 100ms）
constexpr int DEFAULT_EVENT_FRAMES = 10; ///< 默认窗口帧数（1 秒）
constexpr int MAX_EVENT_FRAMES = 600;    ///< 窗口帧数上限（60 秒）

/// @brief 事件映射（events.<事件>）
struct EventConfig {
    bool enabled = false;                        ///< 事件启用（运行期由级联集合覆盖，不写入 user.json）
    EventDataType dataType = EventDataType::STRENGTH; ///< 输出数据类型
    std::string wave;                            ///< 事件附加波形
    MergeRule merge = MergeRule::MULTIPLY;       ///< 与其它波形的合并规则
    double weight = 1.0;                         ///< 合并权重
    std::map<std::string, OptionOutput> options; ///< 子选项映射（键为子选项 id；无声明子选项的事件固定为 default）
    int frames = DEFAULT_EVENT_FRAMES;           ///< 激活窗口帧数（0 及以下按 1 帧，超过 MAX_EVENT_FRAMES 截断）
};

/// @brief 输出端配置（output）
struct OutputConfig {
    std::string wave;                      ///< 输出端附加波形
    MergeRule merge = MergeRule::MULTIPLY; ///< 与合成结果逐帧相乘的合并规则
};

/// @brief 子选项标识（事件 + 子选项）
struct OptionKey {
    std::string event_id;  ///< 事件标识
    std::string option_id; ///< 子选项标识
};

/// @brief 子选项映射条目（互斥检测的输入输出）
/// @note enabled 为运行期状态，不写入 user.json；互斥清理以删除映射条目落盘
struct OptionBinding {
    OptionKey key;       ///< 归属标识
    OptionOutput output; ///< 映射内容
    bool enabled = true; ///< 启用状态（继承事件启用，互斥清理可关闭）
};

/// @brief 操作转文本
/// @param operation 操作枚举
/// @return 契约中的文本值
std::string event_operation_to_string(EventOperation operation);

/// @brief 文本转操作
/// @param text 契约中的文本值
/// @return 对应枚举，未知文本返回 EventOperation::INCREASE
EventOperation event_operation_from_string(const std::string& text);

/// @brief 数值模式转文本
/// @param mode 数值模式枚举
/// @return 契约中的文本值
std::string option_mode_to_string(OptionMode mode);

/// @brief 文本转数值模式
/// @param text 契约中的文本值
/// @return 对应枚举，未知文本返回 OptionMode::VALUE
OptionMode option_mode_from_string(const std::string& text);

/// @brief 合并规则转文本
/// @param rule 合并规则枚举
/// @return 契约中的文本值
std::string merge_rule_to_string(MergeRule rule);

/// @brief 文本转合并规则
/// @param text 契约中的文本值
/// @return 对应枚举，未知文本返回 MergeRule::MULTIPLY
MergeRule merge_rule_from_string(const std::string& text);

/// @brief 输出数据类型转文本
/// @param type 数据类型枚举
/// @return 契约中的文本值
std::string event_data_type_to_string(EventDataType type);

/// @brief 文本转输出数据类型
/// @param text 契约中的文本值
/// @return 对应枚举，未知文本回落 EventDataType::STRENGTH
EventDataType event_data_type_from_string(const std::string& text);

/// @brief 读取全局通道配置（缺失字段取默认值）
/// @param channel 通道（"A"/"B"）
/// @return 通道配置
/// @note 全局键 app.channels.<通道> 缺失时，从旧 app.plugins.<任意模块>.channels.<通道>
///       取首个存在的值写回全局键并返回；此后只读全局键
EventChannelConfig read_channel_config(const std::string& channel);

/// @brief 读取输出端配置（缺失字段取默认值）
/// @param module_name 模块名称
/// @return 输出端配置
OutputConfig read_output_config(const std::string& module_name);

/// @brief 读取模块级默认输出通道（app.plugins.<模块>.channel）
/// @param module_name 模块名称
/// @return "A" / "B" / "AB"；键缺失、类型不符或取值非法时返回 DEFAULT_MODULE_CHANNEL
std::string read_default_channel(const std::string& module_name);

/// @brief 读取单个事件映射（字段缺失取默认值，子选项以声明为基准）
/// @param module_name 模块名称
/// @param definition 事件声明
/// @return 事件映射
/// @note 声明无子选项（options 为空数组）时，映射固定使用隐式键 default
EventConfig read_event_config(const std::string& module_name, const EventDefinition& definition);

/// @brief 读取全部事件映射
/// @param module_name 模块名称
/// @param definitions 事件声明列表
/// @return 事件映射表（键为事件 id）
std::map<std::string, EventConfig> read_event_configs(const std::string& module_name,
    const std::vector<EventDefinition>& definitions);

/// @brief 按运行期启用白名单覆盖事件映射的 enabled（启用级联）
/// @param module_name 模块名称
/// @param enabled_events 运行期启用集合（键写作 "<模块标识>/<事件 ID>"）
/// @param configs 事件映射表（就地覆盖，不落盘）
inline void apply_cascade_enabled(const std::string& module_name,
    const std::set<std::string>& enabled_events, std::map<std::string, EventConfig>& configs) {
    if (module_name.empty() || enabled_events.empty()) {
        return;
    }
    for (auto& [event_id, config] : configs) {
        if (enabled_events.count(module_name + "/" + event_id) != 0) {
            config.enabled = true;
        }
    }
}

/// @brief 读取通道上限（只读访问器，取全局键）
/// @param channel 通道（"A"/"B"）
/// @return 通道上限
int channel_limit(const std::string& channel);

/// @brief 读取通道安全上限（只读访问器，取全局键）
/// @param channel 通道（"A"/"B"）
/// @return 通道安全上限
int channel_safety(const std::string& channel);

/// @brief 写入全局通道配置（写入 user 配置层的 app.channels.<通道>）
/// @param channel 通道（"A"/"B"）
/// @param config 通道配置
void write_channel_config(const std::string& channel, const EventChannelConfig& config);

/// @brief 写入输出端配置（写入 user 配置层）
/// @param module_name 模块名称
/// @param config 输出端配置
void write_output_config(const std::string& module_name, const OutputConfig& config);

/// @brief 写入模块级默认输出通道（写入 user 配置层）
/// @param module_name 模块名称
/// @param channel 通道（"A"/"B"/"AB"）
/// @note 取值非法时按 DEFAULT_MODULE_CHANNEL 落盘
void write_default_channel(const std::string& module_name, const std::string& channel);

/// @brief 写入事件映射（写入 user 配置层，子选项原样落盘）
/// @param module_name 模块名称
/// @param event_id 事件标识
/// @param config 事件映射
void write_event_config(const std::string& module_name, const std::string& event_id,
    const EventConfig& config);

/// @brief 写入单个子选项映射（写入 user 配置层并刷新 edited_at）
/// @param module_name 模块名称
/// @param event_id 事件标识
/// @param option_id 子选项标识
/// @param output 映射内容，edited_at 写回为本次时间戳
void write_option_output(const std::string& module_name, const std::string& event_id,
    const std::string& option_id, OptionOutput& output);

/// @brief 从事件映射表收集子选项条目（启用状态继承事件）
/// @param configs 事件映射表
/// @return 子选项条目列表
std::vector<OptionBinding> collect_option_bindings(const std::map<std::string, EventConfig>& configs);

/// @brief 删除单个子选项映射条目（写入 user 配置层）
/// @param module_name 模块名称
/// @param event_id 事件标识
/// @param option_id 子选项标识
/// @note 删除后该子选项回落默认映射；键不存在、参数为空或配置系统未初始化时静默返回
void remove_option_output(const std::string& module_name, const std::string& event_id,
    const std::string& option_id);

/// @brief 计算并落盘「设为」互斥清理
/// @param module_name 模块名称
/// @param channel 请求清理的通道（"A"/"B"）
/// @param default_channel 模块默认通道（"A"/"B"/"AB"；存储为空串的条目按它展开）
/// @param bindings 子选项条目列表，被关闭项置为 enabled=false
/// @return 被关闭项的展示标识列表
/// @note 先按 resolve_set_conflicts 得出被关闭项，再删除 user.json 中对应映射条目（回落默认映射）
std::vector<std::string> apply_set_conflicts(const std::string& module_name, const std::string& channel,
    const std::string& default_channel, std::vector<OptionBinding>& bindings);

// 互斥检测与清理
#include "EventConflict.h"

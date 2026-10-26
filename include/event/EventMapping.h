/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "EventDefinition.h"

#include <cstdint>
#include <map>
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

/// @brief 通道配置（app.plugins.<模块>.channels.<通道>）
struct EventChannelConfig {
    int limit = 200;  ///< 通道上限
    int safety = 150; ///< 通道安全上限
};

/// @brief 子选项输出映射（events.<事件>.options.<子选项>）
struct OptionOutput {
    std::string channel = "A";                           ///< 目标通道（"A"/"B"）
    EventOperation operation = EventOperation::INCREASE; ///< 操作
    OptionMode mode = OptionMode::VALUE;                 ///< 数值模式
    int value = 0;                                       ///< 绝对值（mode=value 时使用）
    int percent = 0;                                     ///< 百分比（mode=percent 时使用）
    int step = 1;                                        ///< 步进（ramp_* 使用）
    std::int64_t edited_at = 0;                          ///< 最后编辑时间（毫秒时间戳）
};

// 事件激活窗口（1 帧 = 100ms）
constexpr int DEFAULT_EVENT_FRAMES = 10; ///< 默认窗口帧数（1 秒）
constexpr int MAX_EVENT_FRAMES = 600;    ///< 窗口帧数上限（60 秒）

/// @brief 事件映射（events.<事件>）
struct EventConfig {
    bool enabled = false;                        ///< 事件启用
    std::string wave;                            ///< 事件附加波形
    MergeRule merge = MergeRule::MULTIPLY;       ///< 与其它波形的合并规则
    double weight = 1.0;                         ///< 合并权重
    std::map<std::string, OptionOutput> options; ///< 子选项映射（键为子选项 id）
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

/// @brief 读取通道配置（缺失字段取默认值）
/// @param module_name 模块名称
/// @param channel 通道（"A"/"B"）
/// @return 通道配置
EventChannelConfig read_channel_config(const std::string& module_name, const std::string& channel);

/// @brief 读取输出端配置（缺失字段取默认值）
/// @param module_name 模块名称
/// @return 输出端配置
OutputConfig read_output_config(const std::string& module_name);

/// @brief 读取单个事件映射（字段缺失取默认值，子选项以声明为基准）
/// @param module_name 模块名称
/// @param definition 事件声明
/// @return 事件映射
EventConfig read_event_config(const std::string& module_name, const EventDefinition& definition);

/// @brief 读取全部事件映射
/// @param module_name 模块名称
/// @param definitions 事件声明列表
/// @return 事件映射表（键为事件 id）
std::map<std::string, EventConfig> read_event_configs(const std::string& module_name,
    const std::vector<EventDefinition>& definitions);

/// @brief 读取通道上限（只读访问器）
/// @param module_name 模块名称
/// @param channel 通道（"A"/"B"）
/// @return 通道上限
int channel_limit(const std::string& module_name, const std::string& channel);

/// @brief 读取通道安全上限（只读访问器）
/// @param module_name 模块名称
/// @param channel 通道（"A"/"B"）
/// @return 通道安全上限
int channel_safety(const std::string& module_name, const std::string& channel);

/// @brief 写入通道配置（写入 user 配置层）
/// @param module_name 模块名称
/// @param channel 通道（"A"/"B"）
/// @param config 通道配置
void write_channel_config(const std::string& module_name, const std::string& channel,
    const EventChannelConfig& config);

/// @brief 写入输出端配置（写入 user 配置层）
/// @param module_name 模块名称
/// @param config 输出端配置
void write_output_config(const std::string& module_name, const OutputConfig& config);

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

/// @brief 子选项标识转展示文本
/// @param key 子选项标识
/// @return "事件id/子选项id"
std::string option_key_display(const OptionKey& key);

/// @brief 检测同一通道上的「设为」互斥
/// @param channel 通道（"A"/"B"）
/// @param bindings 子选项条目列表
/// @return 该通道上已启用且操作为 set 的条目
std::vector<OptionBinding> detect_set_conflicts(const std::string& channel,
    const std::vector<OptionBinding>& bindings);

/// @brief 清理同一通道上的「设为」互斥（纯计算）
/// @param channel 通道（"A"/"B"）
/// @param bindings 子选项条目列表，被关闭项置为 enabled=false
/// @return 被关闭项的展示标识列表
/// @note 保留 edited_at 最大者，相同时保留标识字典序较小者
/// @note 不读写配置文件；需要落盘删除时用 apply_set_conflicts
std::vector<std::string> resolve_set_conflicts(const std::string& channel,
    std::vector<OptionBinding>& bindings);

/// @brief 删除单个子选项映射条目（写入 user 配置层）
/// @param module_name 模块名称
/// @param event_id 事件标识
/// @param option_id 子选项标识
/// @note 删除后该子选项回落默认映射；键不存在、参数为空或配置系统未初始化时静默返回
void remove_option_output(const std::string& module_name, const std::string& event_id,
    const std::string& option_id);

/// @brief 计算并落盘「设为」互斥清理
/// @param module_name 模块名称
/// @param channel 通道（"A"/"B"）
/// @param bindings 子选项条目列表，被关闭项置为 enabled=false
/// @return 被关闭项的展示标识列表
/// @note 先按 resolve_set_conflicts 得出被关闭项，再删除 user.json 中对应映射条目（回落默认映射）
std::vector<std::string> apply_set_conflicts(const std::string& module_name, const std::string& channel,
    std::vector<OptionBinding>& bindings);

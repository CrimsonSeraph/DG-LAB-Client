/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "AppConfig.h"
#include "EventMapping.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

    const char* const PLUGINS_KEY_PREFIX = "app.plugins."; ///< 映射键前缀
    const char* const USER_CONFIG_NAME = "user";           ///< 写入的配置层

    // 拼接映射键：app.plugins.<模块名>.<后缀>
    std::string mapping_key(const std::string& module_name, const std::string& suffix) {
        std::string key = PLUGINS_KEY_PREFIX;
        key += module_name;
        if (!suffix.empty()) {
            key += ".";
            key += suffix;
        }
        return key;
    }

    // 读取配置子树（缺失或非对象返回空对象）
    nlohmann::json read_subtree(const std::string& key) {
        const nlohmann::json default_value = nlohmann::json::object();
        const nlohmann::json value = AppConfig::instance().get_value<nlohmann::json>(key, default_value);
        return value.is_object() ? value : default_value;
    }

    // 写入配置子树到 user 层（配置系统未初始化时跳过，避免抛出）
    void write_subtree(const std::string& key, const nlohmann::json& value) {
        AppConfig& config = AppConfig::instance();
        if (!config.is_initialized()) {
            return;
        }
        config.set_value_with_name<nlohmann::json>(key, value, USER_CONFIG_NAME);
    }

    // 读取整数字段（缺失或类型不符返回默认值）
    int read_int(const nlohmann::json& object, const char* key, int default_value) {
        const nlohmann::json::const_iterator it = object.find(key);
        if (it == object.end() || !it->is_number()) {
            return default_value;
        }
        return static_cast<int>(it->get<double>());
    }

    // 读取 64 位整数字段（缺失或类型不符返回默认值）
    std::int64_t read_int64(const nlohmann::json& object, const char* key, std::int64_t default_value) {
        const nlohmann::json::const_iterator it = object.find(key);
        if (it == object.end() || !it->is_number()) {
            return default_value;
        }
        return static_cast<std::int64_t>(it->get<double>());
    }

    // 读取浮点字段（缺失或类型不符返回默认值）
    double read_double(const nlohmann::json& object, const char* key, double default_value) {
        const nlohmann::json::const_iterator it = object.find(key);
        if (it == object.end() || !it->is_number()) {
            return default_value;
        }
        return it->get<double>();
    }

    // 读取布尔字段（缺失或类型不符返回默认值）
    bool read_bool(const nlohmann::json& object, const char* key, bool default_value) {
        const nlohmann::json::const_iterator it = object.find(key);
        if (it == object.end() || !it->is_boolean()) {
            return default_value;
        }
        return it->get<bool>();
    }

    // 读取字符串字段（缺失或类型不符返回默认值）
    std::string read_string(const nlohmann::json& object, const char* key, const std::string& default_value) {
        const nlohmann::json::const_iterator it = object.find(key);
        if (it == object.end() || !it->is_string()) {
            return default_value;
        }
        return it->get<std::string>();
    }

    // 事件激活窗口帧数归一（0 及以下按 1 帧，超过上限截断）
    int normalize_frames(int frames) {
        if (frames <= 0) {
            return 1;
        }
        return frames > MAX_EVENT_FRAMES ? MAX_EVENT_FRAMES : frames;
    }

    // 子选项映射 JSON 转值类型
    OptionOutput parse_option_output(const nlohmann::json& object) {
        OptionOutput output;
        output.channel = normalize_channel(read_string(object, "channel", output.channel));
        const std::string operation_text = read_string(object, "operation", std::string());
        if (!operation_text.empty()) {
            output.operation = event_operation_from_string(operation_text);
        }
        const std::string mode_text = read_string(object, "mode", std::string());
        if (!mode_text.empty()) {
            output.mode = option_mode_from_string(mode_text);
        }
        output.value = read_int(object, "value", output.value);
        output.percent = read_int(object, "percent", output.percent);
        output.step = read_int(object, "step", output.step);
        output.edited_at = read_int64(object, "edited_at", output.edited_at);
        return output;
    }

    // 子选项映射值类型转 JSON
    nlohmann::json make_option_output_json(const OptionOutput& output) {
        nlohmann::json object = nlohmann::json::object();
        object["channel"] = normalize_channel(output.channel);
        object["operation"] = event_operation_to_string(output.operation);
        object["mode"] = option_mode_to_string(output.mode);
        object["value"] = output.value;
        object["percent"] = output.percent;
        object["step"] = output.step;
        object["edited_at"] = output.edited_at;
        return object;
    }

    // 当前毫秒时间戳
    std::int64_t now_millis() {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    }

} // namespace

std::string event_operation_to_string(EventOperation operation) {
    switch (operation) {
    case EventOperation::SET: return "set";
    case EventOperation::DECREASE: return "decrease";
    case EventOperation::RAMP_UP: return "ramp_up";
    case EventOperation::RAMP_DOWN: return "ramp_down";
    case EventOperation::INCREASE: break;
    }
    return "increase";
}

EventOperation event_operation_from_string(const std::string& text) {
    if (text == "set") {
        return EventOperation::SET;
    }
    if (text == "decrease") {
        return EventOperation::DECREASE;
    }
    if (text == "ramp_up") {
        return EventOperation::RAMP_UP;
    }
    if (text == "ramp_down") {
        return EventOperation::RAMP_DOWN;
    }
    return EventOperation::INCREASE;
}

std::string option_mode_to_string(OptionMode mode) {
    return mode == OptionMode::PERCENT ? "percent" : "value";
}

OptionMode option_mode_from_string(const std::string& text) {
    return text == "percent" ? OptionMode::PERCENT : OptionMode::VALUE;
}

std::string merge_rule_to_string(MergeRule rule) {
    switch (rule) {
    case MergeRule::MAX: return "max";
    case MergeRule::MIN: return "min";
    case MergeRule::WEIGHTED: return "weighted";
    case MergeRule::MULTIPLY: break;
    }
    return "multiply";
}

MergeRule merge_rule_from_string(const std::string& text) {
    if (text == "max") {
        return MergeRule::MAX;
    }
    if (text == "min") {
        return MergeRule::MIN;
    }
    if (text == "weighted") {
        return MergeRule::WEIGHTED;
    }
    return MergeRule::MULTIPLY;
}

EventChannelConfig read_channel_config(const std::string& module_name, const std::string& channel) {
    EventChannelConfig config;
    if (module_name.empty()) {
        return config;
    }
    const nlohmann::json object = read_subtree(mapping_key(module_name, "channels." + normalize_channel(channel)));
    config.limit = read_int(object, "limit", config.limit);
    config.safety = read_int(object, "safety", config.safety);
    return config;
}

OutputConfig read_output_config(const std::string& module_name) {
    OutputConfig config;
    if (module_name.empty()) {
        return config;
    }
    const nlohmann::json object = read_subtree(mapping_key(module_name, "output"));
    config.wave = read_string(object, "wave", config.wave);
    const std::string merge_text = read_string(object, "merge", std::string());
    if (!merge_text.empty()) {
        config.merge = merge_rule_from_string(merge_text);
    }
    return config;
}

EventConfig read_event_config(const std::string& module_name, const EventDefinition& definition) {
    EventConfig config;
    if (module_name.empty() || definition.id.empty()) {
        return config;
    }
    const nlohmann::json object = read_subtree(mapping_key(module_name, "events." + definition.id));
    config.enabled = read_bool(object, "enabled", config.enabled);
    config.wave = read_string(object, "wave", config.wave);
    const std::string merge_text = read_string(object, "merge", std::string());
    if (!merge_text.empty()) {
        config.merge = merge_rule_from_string(merge_text);
    }
    config.weight = read_double(object, "weight", config.weight);
    config.frames = normalize_frames(read_int(object, "frames", config.frames));
    nlohmann::json options_object = nlohmann::json::object();
    const nlohmann::json::const_iterator options_it = object.find("options");
    if (options_it != object.end() && options_it->is_object()) {
        options_object = *options_it;
    }
    for (const auto& option : definition.options) {
        const nlohmann::json::const_iterator it = options_object.find(option.id);
        if (it != options_object.end() && it->is_object()) {
            config.options[option.id] = parse_option_output(*it);
        } else {
            config.options[option.id] = OptionOutput();
        }
    }
    return config;
}

std::map<std::string, EventConfig> read_event_configs(const std::string& module_name,
    const std::vector<EventDefinition>& definitions) {
    std::map<std::string, EventConfig> configs;
    for (const auto& definition : definitions) {
        if (definition.id.empty()) {
            continue;
        }
        configs[definition.id] = read_event_config(module_name, definition);
    }
    return configs;
}

int channel_limit(const std::string& module_name, const std::string& channel) {
    return read_channel_config(module_name, channel).limit;
}

int channel_safety(const std::string& module_name, const std::string& channel) {
    return read_channel_config(module_name, channel).safety;
}

void write_channel_config(const std::string& module_name, const std::string& channel,
    const EventChannelConfig& config) {
    if (module_name.empty() || channel.empty()) {
        return;
    }
    nlohmann::json object = nlohmann::json::object();
    object["limit"] = config.limit;
    object["safety"] = config.safety;
    write_subtree(mapping_key(module_name, "channels." + normalize_channel(channel)), object);
}

void write_output_config(const std::string& module_name, const OutputConfig& config) {
    if (module_name.empty()) {
        return;
    }
    nlohmann::json object = nlohmann::json::object();
    object["wave"] = config.wave;
    object["merge"] = merge_rule_to_string(config.merge);
    write_subtree(mapping_key(module_name, "output"), object);
}

void write_event_config(const std::string& module_name, const std::string& event_id,
    const EventConfig& config) {
    if (module_name.empty() || event_id.empty()) {
        return;
    }
    nlohmann::json options_object = nlohmann::json::object();
    for (const auto& [option_id, output] : config.options) {
        options_object[option_id] = make_option_output_json(output);
    }
    nlohmann::json object = nlohmann::json::object();
    object["enabled"] = config.enabled;
    object["wave"] = config.wave;
    object["merge"] = merge_rule_to_string(config.merge);
    object["weight"] = config.weight;
    object["frames"] = config.frames;
    object["options"] = options_object;
    write_subtree(mapping_key(module_name, "events." + event_id), object);
}

void write_option_output(const std::string& module_name, const std::string& event_id,
    const std::string& option_id, OptionOutput& output) {
    if (module_name.empty() || event_id.empty() || option_id.empty()) {
        return;
    }
    output.edited_at = now_millis();
    write_subtree(mapping_key(module_name, "events." + event_id + ".options." + option_id),
        make_option_output_json(output));
}

std::vector<OptionBinding> collect_option_bindings(const std::map<std::string, EventConfig>& configs) {
    std::vector<OptionBinding> bindings;
    for (const auto& [event_id, config] : configs) {
        for (const auto& [option_id, output] : config.options) {
            OptionBinding binding;
            binding.key.event_id = event_id;
            binding.key.option_id = option_id;
            binding.output = output;
            binding.enabled = config.enabled;
            bindings.push_back(std::move(binding));
        }
    }
    return bindings;
}

void remove_option_output(const std::string& module_name, const std::string& event_id,
    const std::string& option_id) {
    if (module_name.empty() || event_id.empty() || option_id.empty()) {
        return;
    }
    const std::string key = mapping_key(module_name, "events." + event_id + ".options");
    nlohmann::json options_object = read_subtree(key);
    if (options_object.find(option_id) == options_object.end()) {
        return;
    }
    options_object.erase(option_id);
    write_subtree(key, options_object);
}

std::vector<std::string> apply_set_conflicts(const std::string& module_name, const std::string& channel,
    std::vector<OptionBinding>& bindings) {
    // 记录清理前状态，用于定位本次被关闭的条目
    std::vector<bool> enabled_before;
    enabled_before.reserve(bindings.size());
    for (const auto& binding : bindings) {
        enabled_before.push_back(binding.enabled);
    }
    const std::vector<std::string> cleared = resolve_set_conflicts(channel, bindings);
    for (size_t index = 0; index < bindings.size(); ++index) {
        if (!enabled_before[index] || bindings[index].enabled) {
            continue;
        }
        remove_option_output(module_name, bindings[index].key.event_id, bindings[index].key.option_id);
    }
    return cleared;
}

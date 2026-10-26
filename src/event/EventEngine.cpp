/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "EventEngine.h"

#include "ModuleManager.h"
#include "ModuleValue.h"
#include "WaveLibrary.h"

#include <QString>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

    const char* const CHANNELS[2] = {"A", "B"}; ///< 输出通道标识（索引 0=A，1=B）

    // 通道文本归一为 "A"/"B"（非法通道返回 false）
    bool normalize_channel_text(const std::string& text, std::string& channel) {
        std::string upper = text;
        for (char& ch : upper) {
            if (ch >= 'a' && ch <= 'z') {
                ch = static_cast<char>(ch - 'a' + 'A');
            }
        }
        if (upper != "A" && upper != "B") {
            return false;
        }
        channel = upper;
        return true;
    }

    // 通道索引（"A" -> 0，"B" -> 1，其它返回 -1）
    int channel_index(const std::string& channel) {
        if (channel == "A") {
            return 0;
        }
        if (channel == "B") {
            return 1;
        }
        return -1;
    }

    // 触发判定：按比较方式判断本次数值是否触发事件
    bool compare_matches(const EventTrigger& trigger, int previous, int value) {
        switch (trigger.compare) {
        case EventCompare::ANY:
            return value != previous;
        case EventCompare::INCREASE:
            return (value - previous) >= std::max(1, trigger.amount);
        case EventCompare::DECREASE:
            return (previous - value) >= std::max(1, trigger.amount);
        case EventCompare::CROSS_ABOVE:
            return previous < trigger.amount && value > trigger.amount;
        case EventCompare::CROSS_BELOW:
            return previous > trigger.amount && value < trigger.amount;
        }
        return false;
    }

} // namespace

void EventEngine::configure(const std::string& module_name, const std::vector<EventDefinition>& definitions,
    const std::map<std::string, EventConfig>& configs) {
    if (module_name.empty()) {
        return;
    }
    for (std::map<std::pair<std::string, std::string>, LastValue>::iterator value_it = last_values_.begin();
        value_it != last_values_.end();) {
        if (value_it->first.first == module_name) {
            value_it = last_values_.erase(value_it);
            continue;
        }
        ++value_it;
    }
    ModuleState module;
    module.name = module_name;
    module.default_channel = read_default_channel(module_name);
    for (int index = 0; index < 2; ++index) {
        const EventChannelConfig channel_config = read_channel_config(module_name, CHANNELS[index]);
        module.limits[index] = channel_config.limit;
        module.safeties[index] = channel_config.safety;
    }
    const OutputConfig output_config = read_output_config(module_name);
    module.output_merge = output_config.merge;
    const Wave* output_wave = WaveLibrary::instance().get(output_config.wave);
    if (output_wave != nullptr) {
        module.output_frames = output_wave->frames;
    }
    for (const EventDefinition& definition : definitions) {
        if (definition.id.empty() || definition.trigger.value_id.empty()) {
            continue;
        }
        RuntimeEvent event;
        event.id = definition.id;
        event.label = definition.label;
        event.trigger = definition.trigger;
        const std::map<std::string, EventConfig>::const_iterator config_it = configs.find(definition.id);
        const EventConfig config = config_it == configs.end() ? EventConfig() : config_it->second;
        event.enabled = config.enabled;
        event.merge = config.merge;
        event.weight = config.weight;
        event.window_frames = std::min(std::max(config.frames, 1), MAX_EVENT_FRAMES);
        const ModuleValue* value = ModuleManager::instance().get_value(module_name, definition.trigger.value_id);
        if (value != nullptr) {
            event.min_value = value->get_min();
            event.max_value = value->get_max();
        }
        const Wave* event_wave = WaveLibrary::instance().get(config.wave);
        if (event_wave != nullptr) {
            event.frames = event_wave->frames;
        }
        if (definition.options.empty()) {
            RuntimeOption option;
            option.id = "default";
            const std::map<std::string, OptionOutput>::const_iterator output_it = config.options.find(option.id);
            if (output_it != config.options.end()) {
                option.output = output_it->second;
            }
            event.options.push_back(option);
        }
        else {
            for (const EventOption& declared : definition.options) {
                if (declared.id.empty()) {
                    continue;
                }
                RuntimeOption option;
                option.id = declared.id;
                option.label = declared.label;
                const std::map<std::string, OptionOutput>::const_iterator output_it = config.options.find(declared.id);
                if (output_it != config.options.end()) {
                    option.output = output_it->second;
                }
                event.options.push_back(option);
            }
        }
        for (const RuntimeOption& option : event.options) {
            int indices[2] = {-1, -1};
            const int count = option_channels(option, module, indices);
            for (int index = 0; index < count; ++index) {
                event.on_channel[indices[index]] = true;
            }
        }
        module.events[event.id] = event;
    }
    modules_[module_name] = module;
}

void EventEngine::clear() {
    for (std::map<std::string, ModuleState>::iterator module_it = modules_.begin();
        module_it != modules_.end(); ++module_it) {
        for (std::map<std::string, RuntimeEvent>::iterator event_it = module_it->second.events.begin();
            event_it != module_it->second.events.end(); ++event_it) {
            RuntimeEvent& event = event_it->second;
            event.triggered = false;
            event.remaining_frames = 0;
            event.channels[0] = ChannelState();
            event.channels[1] = ChannelState();
        }
    }
    last_values_.clear();
    channel_outputs_.clear();
    last_time_ms_ = 0;
    frame_index_ = 0;
}

void EventEngine::on_value_changed(const std::string& module_name, const std::string& value_id, int value) {
    LastValue& last = last_values_[std::make_pair(module_name, value_id)];
    if (!last.valid) {
        last.valid = true;
        last.value = value;
        return;
    }
    const int previous = last.value;
    last.value = value;
    if (previous == value) {
        return;
    }
    const std::map<std::string, ModuleState>::iterator module_it = modules_.find(module_name);
    if (module_it == modules_.end()) {
        return;
    }
    ModuleState& module = module_it->second;
    for (std::map<std::string, RuntimeEvent>::iterator event_it = module.events.begin();
        event_it != module.events.end(); ++event_it) {
        RuntimeEvent& event = event_it->second;
        if (!event.enabled || event.trigger.value_id != value_id) {
            continue;
        }
        if (!compare_matches(event.trigger, previous, value)) {
            continue;
        }
        event.triggered = true;
        event.remaining_frames = event.window_frames;
        const RuntimeOption* set_choices[2] = {nullptr, nullptr};
        for (const RuntimeOption& option : event.options) {
            if (option.output.operation != EventOperation::SET) {
                continue;
            }
            int indices[2] = {-1, -1};
            const int count = option_channels(option, module, indices);
            for (int position = 0; position < count; ++position) {
                const RuntimeOption*& chosen = set_choices[indices[position]];
                if (chosen == nullptr || option.output.edited_at > chosen->output.edited_at
                    || (option.output.edited_at == chosen->output.edited_at && option.id < chosen->id)) {
                    chosen = &option;
                }
            }
        }
        for (const RuntimeOption& option : event.options) {
            int indices[2] = {-1, -1};
            const int count = option_channels(option, module, indices);
            for (int position = 0; position < count; ++position) {
                const int index = indices[position];
                if (option.output.operation == EventOperation::SET && set_choices[index] != &option) {
                    continue;
                }
                apply_option(module, event, option, CHANNELS[index]);
            }
        }
    }
}

void EventEngine::advance_frame(std::int64_t now_ms) {
    last_time_ms_ = now_ms;
    ++frame_index_;
    for (int index = 0; index < 2; ++index) {
        const std::string channel = CHANNELS[index];
        double accumulator = 0.0;
        bool has_primary = false;
        double limit = 0.0;
        int safety = 0;
        const QStringList* output_frames = nullptr;
        MergeRule output_merge = MergeRule::MULTIPLY;
        for (std::map<std::string, ModuleState>::iterator module_it = modules_.begin();
            module_it != modules_.end(); ++module_it) {
            ModuleState& module = module_it->second;
            const int module_limit = module.limits[index];
            for (std::map<std::string, RuntimeEvent>::iterator event_it = module.events.begin();
                event_it != module.events.end(); ++event_it) {
                RuntimeEvent& event = event_it->second;
                if (!event.enabled || !event.triggered || !event.on_channel[index]) {
                    continue;
                }
                ChannelState& state = event.channels[index];
                double strength = state.base + state.increase_sum - state.decrease_sum + advance_ramps(state);
                if (strength < 0.0) {
                    strength = 0.0;
                }
                if (strength > static_cast<double>(module_limit)) {
                    strength = static_cast<double>(module_limit);
                }
                strength *= envelope_of(event.frames);
                const double ratio = module_limit > 0 ? strength / static_cast<double>(module_limit) : 0.0;
                if (!has_primary) {
                    has_primary = true;
                    accumulator = ratio;
                    limit = static_cast<double>(module_limit);
                    safety = module.safeties[index];
                    output_frames = &module.output_frames;
                    output_merge = module.output_merge;
                }
                else {
                    accumulator = fold_merge(accumulator, ratio, event.merge, event.weight);
                }
            }
        }
        if (!has_primary || limit <= 0.0) {
            channel_outputs_[channel] = 0;
            continue;
        }
        double merged = accumulator;
        if (output_frames != nullptr && !output_frames->isEmpty()) {
            merged = apply_merge(output_merge, {merged, envelope_of(*output_frames)}, {});
        }
        const double strength = merged * limit;
        channel_outputs_[channel] = clamp_strength(static_cast<int>(std::lround(strength)),
            static_cast<int>(limit), safety);
    }
    expire_events();
}

int EventEngine::channel_output(const std::string& channel) const {
    std::string normalized;
    if (!normalize_channel_text(channel, normalized)) {
        return 0;
    }
    const std::map<std::string, int>::const_iterator it = channel_outputs_.find(normalized);
    return it == channel_outputs_.end() ? 0 : it->second;
}

std::vector<TriggerRecord> EventEngine::recent_triggers(int limit) const {
    std::vector<TriggerRecord> records;
    if (limit <= 0) {
        return records;
    }
    const std::size_t count = std::min(static_cast<std::size_t>(limit), triggers_.size());
    const std::size_t start = triggers_.size() - count;
    records.reserve(count);
    for (std::size_t index = start; index < triggers_.size(); ++index) {
        records.push_back(triggers_[index]);
    }
    return records;
}

int EventEngine::option_channels(const RuntimeOption& option, const ModuleState& module, int indices[2]) const {
    std::string channel;
    if (normalize_channel_text(option.output.channel, channel)) {
        indices[0] = channel_index(channel);
        return 1;
    }
    if (!option.output.channel.empty()) {
        return 0;
    }
    // 空串表示跟随模块默认通道：A/B 单个通道，AB 两个通道各一次
    int count = 0;
    if (module.default_channel != "B") {
        indices[count] = 0;
        ++count;
    }
    if (module.default_channel != "A") {
        indices[count] = 1;
        ++count;
    }
    return count;
}

double EventEngine::option_mode_value(const RuntimeEvent& event, const OptionOutput& output, int limit) const {
    const double channel_limit = static_cast<double>(limit);
    if (output.mode == OptionMode::PERCENT) {
        return static_cast<double>(output.percent) / 100.0 * channel_limit;
    }
    const int min_value = event.min_value.value_or(0);
    const int max_value = event.max_value.value_or(limit);
    return normalize_value(output.value, min_value, max_value) * channel_limit;
}

void EventEngine::apply_option(ModuleState& module, RuntimeEvent& event, const RuntimeOption& option,
    const std::string& channel) {
    const int index = channel_index(channel);
    if (index < 0) {
        return;
    }
    ChannelState& state = event.channels[index];
    const double mode_value = option_mode_value(event, option.output, module.limits[index]);
    int delta = 0;
    switch (option.output.operation) {
    case EventOperation::SET: {
        const int previous = static_cast<int>(std::lround(state.base));
        state.base = mode_value;
        delta = static_cast<int>(std::lround(state.base)) - previous;
        break;
    }
    case EventOperation::INCREASE:
        state.increase_sum += mode_value;
        delta = static_cast<int>(std::lround(mode_value));
        break;
    case EventOperation::DECREASE:
        state.decrease_sum += mode_value;
        delta = -static_cast<int>(std::lround(mode_value));
        break;
    case EventOperation::RAMP_UP:
    case EventOperation::RAMP_DOWN: {
        const double direction = option.output.operation == EventOperation::RAMP_UP ? 1.0 : -1.0;
        const double step = std::abs(static_cast<double>(option.output.step));
        const double target = std::abs(mode_value);
        delta = static_cast<int>(std::lround(direction * step));
        if (step > 0.0 && target > 0.0) {
            RampState ramp;
            ramp.step = direction * step;
            ramp.target = direction * target;
            state.ramps.push_back(ramp);
        }
        break;
    }
    }
    TriggerRecord record;
    record.timestamp_ms = last_time_ms_;
    record.module_name = module.name;
    record.event_id = event.id;
    record.event_label = event.label;
    record.option_id = option.id;
    record.option_label = option.label;
    record.channel = channel;
    record.operation = event_operation_to_string(option.output.operation);
    record.delta = delta;
    push_trigger(record);
}

double EventEngine::advance_ramps(ChannelState& state) const {
    double total = 0.0;
    std::size_t index = 0;
    while (index < state.ramps.size()) {
        RampState& ramp = state.ramps[index];
        ++ramp.frames;
        ramp.current += ramp.step;
        bool finished = false;
        if (ramp.step > 0.0) {
            if (ramp.current >= ramp.target) {
                ramp.current = ramp.target;
                finished = true;
            }
        }
        else if (ramp.current <= ramp.target) {
            ramp.current = ramp.target;
            finished = true;
        }
        if (ramp.frames >= RAMP_MAX_FRAMES) {
            finished = true;
        }
        total += ramp.current;
        if (finished) {
            state.ramps.erase(state.ramps.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        ++index;
    }
    return total;
}

void EventEngine::expire_events() {
    for (std::map<std::string, ModuleState>::iterator module_it = modules_.begin();
        module_it != modules_.end(); ++module_it) {
        for (std::map<std::string, RuntimeEvent>::iterator event_it = module_it->second.events.begin();
            event_it != module_it->second.events.end(); ++event_it) {
            RuntimeEvent& event = event_it->second;
            if (!event.triggered) {
                continue;
            }
            if (!event.channels[0].ramps.empty() || !event.channels[1].ramps.empty()) {
                continue;
            }
            --event.remaining_frames;
            if (event.remaining_frames > 0) {
                continue;
            }
            event.remaining_frames = 0;
            event.triggered = false;
            event.channels[0] = ChannelState();
            event.channels[1] = ChannelState();
        }
    }
}

double EventEngine::envelope_of(const QStringList& frames) const {
    if (frames.isEmpty()) {
        return 1.0;
    }
    const std::int64_t count = static_cast<std::int64_t>(frames.size());
    const std::int64_t position = frame_index_ > 0 ? frame_index_ - 1 : 0;
    const int index = static_cast<int>(position % count);
    return static_cast<double>(wave_envelope_percent(frames.at(index))) / 100.0;
}

double EventEngine::fold_merge(double accumulator, double value, MergeRule rule, double weight) const {
    return apply_merge(rule, {accumulator, value}, {1.0, weight});
}

void EventEngine::push_trigger(const TriggerRecord& record) {
    triggers_.push_back(record);
    while (triggers_.size() > static_cast<std::size_t>(TRIGGER_CAPACITY)) {
        triggers_.pop_front();
    }
}

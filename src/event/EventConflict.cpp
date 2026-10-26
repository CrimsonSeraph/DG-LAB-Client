/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "EventConflict.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

    // 子选项标识字典序比较（事件 id 优先）
    bool option_key_less(const OptionKey& left, const OptionKey& right) {
        if (left.event_id != right.event_id) {
            return left.event_id < right.event_id;
        }
        return left.option_id < right.option_id;
    }

    // 模块默认通道归一（"A"/"B"/"AB"，其它回落 "AB"）
    std::string normalize_default_channel(const std::string& channel) {
        const std::string upper = normalize_channel(channel);
        if (upper == "A" || upper == "B" || upper == "AB") {
            return upper;
        }
        return DEFAULT_MODULE_CHANNEL;
    }

    // 条目实际生效的通道（空串表示跟随模块默认通道，AB 展开为 A 与 B）
    std::vector<std::string> effective_channels(const std::string& channel, const std::string& default_channel) {
        std::vector<std::string> channels;
        const std::string stored = normalize_channel(channel);
        if (stored == "A" || stored == "B") {
            channels.push_back(stored);
            return channels;
        }
        if (!stored.empty()) {
            return channels;
        }
        const std::string target = normalize_default_channel(default_channel);
        if (target != "B") {
            channels.push_back("A");
        }
        if (target != "A") {
            channels.push_back("B");
        }
        return channels;
    }

    // 通道集合是否包含指定通道
    bool contains_channel(const std::vector<std::string>& channels, const std::string& target) {
        for (const std::string& channel : channels) {
            if (channel == target) {
                return true;
            }
        }
        return false;
    }

    // 条目是否为指定通道上的候选（已启用、操作为 set、实际生效通道包含该通道）
    bool is_conflict_candidate(const OptionBinding& binding, const std::string& default_channel,
        const std::string& target) {
        if (!binding.enabled || binding.output.operation != EventOperation::SET) {
            return false;
        }
        return contains_channel(effective_channels(binding.output.channel, default_channel), target);
    }

} // namespace

std::string normalize_channel(const std::string& channel) {
    std::string result = channel;
    for (char& ch : result) {
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 'a' + 'A');
        }
    }
    return result;
}

std::string option_key_display(const OptionKey& key) {
    return key.event_id + "/" + key.option_id;
}

std::vector<OptionBinding> detect_set_conflicts(const std::string& channel, const std::string& default_channel,
    const std::vector<OptionBinding>& bindings) {
    std::vector<OptionBinding> conflicts;
    const std::string target = normalize_channel(channel);
    if (target != "A" && target != "B") {
        return conflicts;
    }
    for (const OptionBinding& binding : bindings) {
        if (!is_conflict_candidate(binding, default_channel, target)) {
            continue;
        }
        conflicts.push_back(binding);
    }
    return conflicts;
}

std::vector<SetConflictLoss> resolve_set_conflicts(const std::string& channel, const std::string& default_channel,
    std::vector<OptionBinding>& bindings) {
    std::vector<SetConflictLoss> losses;
    const std::string target = normalize_channel(channel);
    if (target != "A" && target != "B") {
        return losses;
    }
    // 选出保留项：edited_at 最大，相同时标识字典序最小
    std::optional<size_t> keep_index;
    for (size_t index = 0; index < bindings.size(); ++index) {
        const OptionBinding& binding = bindings[index];
        if (!is_conflict_candidate(binding, default_channel, target)) {
            continue;
        }
        if (!keep_index.has_value()) {
            keep_index = index;
            continue;
        }
        const OptionBinding& keep_binding = bindings[keep_index.value()];
        if (binding.output.edited_at > keep_binding.output.edited_at) {
            keep_index = index;
            continue;
        }
        const bool same_time = binding.output.edited_at == keep_binding.output.edited_at;
        if (same_time && option_key_less(binding.key, keep_binding.key)) {
            keep_index = index;
        }
    }
    if (!keep_index.has_value()) {
        return losses;
    }
    for (size_t index = 0; index < bindings.size(); ++index) {
        if (index == keep_index.value()) {
            continue;
        }
        OptionBinding& binding = bindings[index];
        if (!is_conflict_candidate(binding, default_channel, target)) {
            continue;
        }
        binding.enabled = false;
        SetConflictLoss loss;
        loss.key = binding.key;
        loss.channels = effective_channels(binding.output.channel, default_channel);
        losses.push_back(std::move(loss));
    }
    return losses;
}

/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "EventConflict.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace {

    // 子选项标识字典序比较（事件 id 优先）
    bool option_key_less(const OptionKey& left, const OptionKey& right) {
        if (left.event_id != right.event_id) {
            return left.event_id < right.event_id;
        }
        return left.option_id < right.option_id;
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

std::vector<OptionBinding> detect_set_conflicts(const std::string& channel,
    const std::vector<OptionBinding>& bindings) {
    std::vector<OptionBinding> conflicts;
    const std::string target = normalize_channel(channel);
    for (const auto& binding : bindings) {
        if (!binding.enabled || binding.output.operation != EventOperation::SET) {
            continue;
        }
        if (normalize_channel(binding.output.channel) != target) {
            continue;
        }
        conflicts.push_back(binding);
    }
    return conflicts;
}

std::vector<std::string> resolve_set_conflicts(const std::string& channel,
    std::vector<OptionBinding>& bindings) {
    std::vector<std::string> disabled;
    const std::string target = normalize_channel(channel);
    // 选出保留项：edited_at 最大，相同时标识字典序最小
    std::optional<size_t> keep_index;
    for (size_t index = 0; index < bindings.size(); ++index) {
        const OptionBinding& binding = bindings[index];
        if (!binding.enabled || binding.output.operation != EventOperation::SET) {
            continue;
        }
        if (normalize_channel(binding.output.channel) != target) {
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
        return disabled;
    }
    for (size_t index = 0; index < bindings.size(); ++index) {
        if (index == keep_index.value()) {
            continue;
        }
        OptionBinding& binding = bindings[index];
        if (!binding.enabled || binding.output.operation != EventOperation::SET) {
            continue;
        }
        if (normalize_channel(binding.output.channel) != target) {
            continue;
        }
        binding.enabled = false;
        disabled.push_back(option_key_display(binding.key));
    }
    return disabled;
}

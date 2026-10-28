/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "EventMath.h"

#include <QString>

#include <algorithm>
#include <cstddef>
#include <vector>

int clamp_strength(int value, int limit, int safety) {
    const int upper = std::max(0, std::min(limit, safety));
    if (value < 0) {
        return 0;
    }
    if (value > upper) {
        return upper;
    }
    return value;
}

double apply_merge(MergeRule rule, const std::vector<double>& values, const std::vector<double>& weights) {
    if (values.empty()) {
        return 0.0;
    }
    switch (rule) {
    case MergeRule::MULTIPLY: {
        double product = 1.0;
        for (const double value : values) {
            product *= value;
        }
        return product;
    }
    case MergeRule::MAX: {
        double result = values.front();
        for (const double value : values) {
            result = std::max(result, value);
        }
        return result;
    }
    case MergeRule::MIN: {
        double result = values.front();
        for (const double value : values) {
            result = std::min(result, value);
        }
        return result;
    }
    case MergeRule::WEIGHTED: {
        double weighted_sum = 0.0;
        double weight_sum = 0.0;
        for (std::size_t index = 0; index < values.size(); ++index) {
            const double weight = index < weights.size() ? weights[index] : 1.0;
            weighted_sum += weight * values[index];
            weight_sum += weight;
        }
        if (weight_sum == 0.0) {
            double sum = 0.0;
            for (const double value : values) {
                sum += value;
            }
            return sum / static_cast<double>(values.size());
        }
        return weighted_sum / weight_sum;
    }
    }
    return 0.0;
}

double normalize_value(int value, int min_value, int max_value) {
    if (max_value <= min_value) {
        return 0.0;
    }
    const double ratio = static_cast<double>(value - min_value) / static_cast<double>(max_value - min_value);
    if (ratio < 0.0) {
        return 0.0;
    }
    if (ratio > 1.0) {
        return 1.0;
    }
    return ratio;
}

int wave_envelope_percent(const QString& frame) {
    if (frame.size() < 16) {
        return 0;
    }
    const QString strength_text = frame.mid(8, 8);
    int sum = 0;
    for (int index = 0; index < 4; ++index) {
        bool ok = false;
        const int strength = strength_text.mid(index * 2, 2).toInt(&ok, 16);
        if (!ok) {
            return 0;
        }
        sum += strength;
    }
    return std::min(100, sum / 4);
}

int ChannelOutputState::final_value(int limit) const {
    return clamp_strength(std::max(rule_value, event_value), limit, limit);
}

bool ChannelOutputState::take_output(int limit, int& value) {
    const int merged = final_value(limit);
    if (merged == last_sent) {
        return false;
    }
    last_sent = merged;
    value = merged;
    return true;
}

void ChannelOutputState::reset_output() {
    last_sent = -1;
}

#include "Rule.h"
#include "DebugLog.h"

Rule::Rule(const std::string& name, const std::string& pattern)
    : name_(name), pattern_(pattern) {
    parse_pattern();
}

void Rule::parse_pattern() {
    placeholderPositions_.clear();
    placeholderCount_ = 0;
    std::string::size_type pos = 0;
    while ((pos = pattern_.find("{}", pos)) != std::string::npos) {
        placeholderPositions_.push_back(pos);
        ++placeholderCount_;
        pos += 2; // 跳过 {}
    }
}

size_t Rule::get_placeholder_count() const {
    return placeholderCount_;
}

std::string Rule::evaluate(const std::vector<int>& values) const {
    if (values.size() != placeholderCount_) {
        LOG_MODULE("Rule", "evaluate", LOG_ERROR,
            "规则 " << name_ << " 需要 " << placeholderCount_ << " 个参数，实际收到 " << values.size());
        throw std::invalid_argument("参数数量不匹配");
    }

    std::string result = pattern_;
    size_t offset = 0;
    for (size_t i = 0; i < placeholderCount_; ++i) {
        size_t pos = placeholderPositions_[i] + offset;
        std::string valStr = std::to_string(values[i]);
        result.replace(pos, 2, valStr);
        offset += valStr.length() - 2; // 调整后续偏移
    }
    return result;
}

std::string Rule::get_display_string() const {
    // 将 pattern 中每个 {} 替换为 "{   }"（三个空格）
    std::string display = pattern_;
    std::string placeholder = "{   }";
    size_t pos = 0;
    while ((pos = display.find("{}", pos)) != std::string::npos) {
        display.replace(pos, 2, placeholder);
        pos += placeholder.length();
    }
    return name_ + " = " + display;
}

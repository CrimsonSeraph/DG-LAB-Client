#pragma once

#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class Rule {
public:
    Rule() = default;
    Rule(const std::string& name, const std::string& pattern);

    // 获取规则名称
    const std::string& getName() const { return name_; }

    // 获取模式字符串
    const std::string& getPattern() const { return pattern_; }

    // 获取占位符数量（即 {} 的个数）
    size_t get_placeholder_count() const;

    // 评估规则：将 values 按顺序替换到 {} 中，返回结果字符串
    std::string evaluate(const std::vector<int>& values) const;

    // 获取用于 UI 显示的字符串：规则名称 + "=" + 每个 {} 替换为 "{   }"（内部三个空格）
    std::string get_display_string() const;

private:
    std::string name_;
    std::string pattern_;
    std::vector<size_t> placeholderPositions_; // 缓存 {} 的位置，提高性能
    size_t placeholderCount_ = 0;

    void parse_pattern(); // 解析 pattern，记录 {} 的位置和个数
};

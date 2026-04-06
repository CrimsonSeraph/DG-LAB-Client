#pragma once

#include <QJsonObject>

#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class Rule {
public:
    Rule() = default;
    Rule(const std::string& name,
        const std::string& channel,
        int mode,
        const std::string& value_pattern);

    // 获取基本属性
    inline const std::string& get_name() const { return name_; }
    inline const std::string& get_channel() const { return channel_; }
    inline int get_mode() const { return mode_; }
    inline const std::string& get_value_pattern() const { return value_pattern_; }

    // 规范化通道输入，接受 "A", "a", "B", "b"，返回 "A", "B" 或 ""（无效输入）
    static std::string normalize_channel(const std::string& channel);

    // 获取占位符数量
    size_t get_placeholder_count() const;

    // 计算值：将参数填入 value_pattern，返回整数结果
    int compute_value(const std::vector<int>& values) const;

    // 生成完整的 QJsonObject 命令
    QJsonObject generate_command(const std::vector<int>& values) const;

    // 获取用于 UI 显示的字符串
    std::string get_display_string() const;

private:
    std::string name_;
    std::string channel_;   // "A", "B", ""
    int mode_;  // 0-4
    std::string value_pattern_; // 包含 {} 的表达式
    std::vector<size_t> placeholder_positions_;
    size_t placeholder_count_ = 0;

    void parse_pattern();   // 解析 value_pattern_ 中的 {}
    std::string evaluate_value_pattern(const std::vector<int>& values) const;   // 返回替换后的字符串
};

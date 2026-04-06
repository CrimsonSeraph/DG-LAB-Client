#pragma once

#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <QJsonObject>

class Rule {
public:
    Rule() = default;
    Rule(const std::string& name,
        const std::string& channel,
        int mode,
        const std::string& valuePattern);

    // 获取基本属性
    inline const std::string& getName() const { return name_; }
    inline const std::string& getChannel() const { return channel_; }
    inline int getMode() const { return mode_; }
    inline const std::string& getValuePattern() const { return valuePattern_; }

    // 规范化通道输入，接受 "A", "a", "B", "b"，返回 "A", "B" 或 ""（无效输入）
    static std::string normalizeChannel(const std::string& channel);

    // 获取占位符数量
    size_t get_placeholder_count() const;

    // 计算值：将参数填入 valuePattern，返回整数结果
    int computeValue(const std::vector<int>& params) const;

    // 生成完整的 QJsonObject 命令
    QJsonObject generateCommand(const std::vector<int>& params) const;

    // 获取用于 UI 显示的字符串
    std::string get_display_string() const;

private:
    std::string name_;
    std::string channel_;   // "A", "B", ""
    int mode_;  // 0-4
    std::string valuePattern_;  // 包含 {} 的表达式
    std::vector<size_t> placeholderPositions_;
    size_t placeholderCount_ = 0;

    void parse_pattern();   // 解析 valuePattern 中的 {}
    std::string evaluateValuePattern(const std::vector<int>& values) const; // 返回替换后的字符串
};

/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QDialog>

#include <string>
#include <vector>

class QComboBox;
class QListWidget;

// ============================================
// ParentEditDialog - 规则父级编辑对话框
// 通道父级（A/B/无）单选 + 规则父级（引用 {rule:xx}）多选
// ============================================
class ParentEditDialog : public QDialog {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    /// @brief 构造函数
    /// @param rule_name 当前编辑的规则名称
    /// @param parent 父窗口指针
    explicit ParentEditDialog(const std::string& rule_name, QWidget* parent = nullptr);

    // -------------------- 公共接口（获取编辑结果）--------------------
    /// @brief 获取选择的通道父级
    /// @return 通道（"A"/"B"/空字符串表示无）
    std::string get_channel() const;

    /// @brief 获取勾选的规则父级序号列表
    /// @return 规则序号列表
    std::vector<int> get_selected_rules() const;

private:
    // -------------------- 成员变量 --------------------
    std::string rule_name_;            ///< 当前编辑的规则名称
    QComboBox* channel_combo_ = nullptr; ///< 通道下拉框（无/A/B）
    QListWidget* rules_list_ = nullptr;  ///< 规则多选列表

    // -------------------- 私有辅助函数 --------------------
    /// @brief 构建界面（通道单选 + 规则多选）
    void setup_ui();
};

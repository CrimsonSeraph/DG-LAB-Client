#pragma once

#include <QString>
#include <QStringList>

struct Wave;

// TagRule - 内置标签的自动判定规则
// 对波形做客观分析（时长、频率分布、来源等），返回应自动挂载的内置标签
// 只覆盖可客观判定的分类
// 语义类标签仍由 TagCatalog::official_wave_tag_table() 手动指定
namespace TagRule {

    /// @brief 汇总波形满足的全部自动标签
    /// @param wave 波形
    /// @return 已规范化、大小写不敏感去重的自动标签列表
    QStringList auto_tags_for(const Wave& wave);

    /// @brief 该标签是否由规则自动判定（用于界面徽标 / 日志）
    /// @param tag 标签名
    /// @return 是否属于自动标签集合
    bool is_auto_tag(const QString& tag);

} // namespace TagRule

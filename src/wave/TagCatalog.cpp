/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "TagCatalog.h"

#include "Wave.h"

#include <map>

namespace {

    // 内置分类与内置标签：七类固定，顺序即波形上标签的显示顺序。
    // 各分类下的标签按官方 App 的标签清单逐条追加，未追加时为空列表。
    // 「用户自定义」是内置分类（名字不可改）：用户在该分类下新建的标签属用户侧数据，不写在这里。
    const std::vector<TagCategoryDef>& category_table() {
        static const std::vector<TagCategoryDef> table = {
            {QStringLiteral("使用部位"), {
                                             QStringLiteral("肌肉（臀/腿/腹）"),
                                             QStringLiteral("脚底"),
                                             QStringLiteral("平滑肌（隐私部位）"),
                                             QStringLiteral("体内（直肠/阴道）"),
                                         }},
            {QStringLiteral("使用场景"), {
                                             QStringLiteral("前戏挑逗"),
                                             QStringLiteral("刺激惩罚"),
                                             QStringLiteral("高潮助力"),
                                             QStringLiteral("按摩放松"),
                                         }},
            {QStringLiteral("频率风格"), {
                                             QStringLiteral("纯高频"),
                                             QStringLiteral("高频"),
                                             QStringLiteral("低频"),
                                             QStringLiteral("混合"),
                                         }},
            {QStringLiteral("波形时长"), {
                                             QStringLiteral("10 秒以下"),
                                             QStringLiteral("10 秒-1 分钟"),
                                             QStringLiteral("1-5 分钟"),
                                             QStringLiteral("5 分钟以上"),
                                         }},
            {QStringLiteral("播放速率"), {
                                             QStringLiteral("1X"),
                                             QStringLiteral("2X"),
                                             QStringLiteral("4X"),
                                         }},
            {QStringLiteral("波形来源"), {
                                             QStringLiteral("经典波形"),
                                             QStringLiteral("自定义波形"),
                                         }},
            {QStringLiteral("用户自定义"), {}},
        };
        return table;
    }

    // 官方波形名 → 内置标签：内置波形每次加载后按此表自动挂载，界面锁定不可移除。
    // 条目按官方 App 的标签清单逐条追加，未追加时为空表。
    const std::map<QString, QStringList>& official_wave_tag_table() {
        static const std::map<QString, QStringList> table = {
            {QStringLiteral("信号灯"), {
                                           QStringLiteral("脚底"),
                                           QStringLiteral("前戏挑逗"),
                                           QStringLiteral("低频"),
                                           QStringLiteral("10 秒以下"),
                                           QStringLiteral("1X"),
                                           QStringLiteral("经典波形"),
                                       }},
            {QStringLiteral("压缩"), {
                                         QStringLiteral("肌肉（臀/腿/腹）"),
                                         QStringLiteral("刺激惩罚"),
                                         QStringLiteral("高频"),
                                         QStringLiteral("10 秒以下"),
                                         QStringLiteral("1X"),
                                         QStringLiteral("经典波形"),
                                     }},
            {QStringLiteral("变速敲击"), {
                                             QStringLiteral("肌肉（臀/腿/腹）"),
                                             QStringLiteral("刺激惩罚"),
                                             QStringLiteral("低频"),
                                             QStringLiteral("10 秒以下"),
                                             QStringLiteral("1X"),
                                             QStringLiteral("经典波形"),
                                         }},
            {QStringLiteral("呼吸"), {
                                         QStringLiteral("体内（直肠/阴道）"),
                                         QStringLiteral("按摩放松"),
                                         QStringLiteral("纯高频"),
                                         QStringLiteral("10 秒以下"),
                                         QStringLiteral("1X"),
                                         QStringLiteral("经典波形"),
                                     }},
            {QStringLiteral("心跳节奏"), {
                                             QStringLiteral("平滑肌（隐私部位）"),
                                             QStringLiteral("前戏挑逗"),
                                             QStringLiteral("混合"),
                                             QStringLiteral("10 秒以下"),
                                             QStringLiteral("1X"),
                                             QStringLiteral("经典波形"),
                                         }},
            {QStringLiteral("快速按捏"), {
                                             QStringLiteral("肌肉（臀/腿/腹）"),
                                             QStringLiteral("刺激惩罚"),
                                             QStringLiteral("纯高频"),
                                             QStringLiteral("10 秒以下"),
                                             QStringLiteral("1X"),
                                             QStringLiteral("经典波形"),
                                         }},
            {QStringLiteral("按捏渐强"), {
                                             QStringLiteral("肌肉（臀/腿/腹）"),
                                             QStringLiteral("刺激惩罚"),
                                             QStringLiteral("纯高频"),
                                             QStringLiteral("10 秒以下"),
                                             QStringLiteral("1X"),
                                             QStringLiteral("经典波形"),
                                         }},
            {QStringLiteral("挑逗1"), {
                                          QStringLiteral("平滑肌（隐私部位）"),
                                          QStringLiteral("前戏挑逗"),
                                          QStringLiteral("高频"),
                                          QStringLiteral("10 秒以下"),
                                          QStringLiteral("1X"),
                                          QStringLiteral("经典波形"),
                                      }},
            {QStringLiteral("挑逗2"), {
                                          QStringLiteral("平滑肌（隐私部位）"),
                                          QStringLiteral("前戏挑逗"),
                                          QStringLiteral("高频"),
                                          QStringLiteral("10 秒以下"),
                                          QStringLiteral("1X"),
                                          QStringLiteral("经典波形"),
                                      }},
            {QStringLiteral("波浪涟漪"), {
                                             QStringLiteral("平滑肌（隐私部位）"),
                                             QStringLiteral("前戏挑逗"),
                                             QStringLiteral("混合"),
                                             QStringLiteral("10 秒以下"),
                                             QStringLiteral("1X"),
                                             QStringLiteral("经典波形"),
                                         }},
            {QStringLiteral("渐变弹跳"), {
                                             QStringLiteral("脚底"),
                                             QStringLiteral("前戏挑逗"),
                                             QStringLiteral("高频"),
                                             QStringLiteral("10 秒以下"),
                                             QStringLiteral("1X"),
                                             QStringLiteral("经典波形"),
                                         }},
            {QStringLiteral("潮汐"), {
                                         QStringLiteral("体内（直肠/阴道）"),
                                         QStringLiteral("按摩放松"),
                                         QStringLiteral("高频"),
                                         QStringLiteral("10 秒以下"),
                                         QStringLiteral("1X"),
                                         QStringLiteral("经典波形"),
                                     }},
            {QStringLiteral("节奏步伐"), {
                                             QStringLiteral("脚底"),
                                             QStringLiteral("前戏挑逗"),
                                             QStringLiteral("纯高频"),
                                             QStringLiteral("10 秒以下"),
                                             QStringLiteral("1X"),
                                             QStringLiteral("经典波形"),
                                         }},
            {QStringLiteral("连击"), {
                                         QStringLiteral("肌肉（臀/腿/腹）"),
                                         QStringLiteral("刺激惩罚"),
                                         QStringLiteral("纯高频"),
                                         QStringLiteral("10 秒以下"),
                                         QStringLiteral("1X"),
                                         QStringLiteral("经典波形"),
                                     }},
            {QStringLiteral("雨水冲刷"), {
                                             QStringLiteral("脚底"),
                                             QStringLiteral("按摩放松"),
                                             QStringLiteral("混合"),
                                             QStringLiteral("10 秒以下"),
                                             QStringLiteral("1X"),
                                             QStringLiteral("经典波形"),
                                         }},
            {QStringLiteral("颗粒摩擦"), {
                                             QStringLiteral("肌肉（臀/腿/腹）"),
                                             QStringLiteral("刺激惩罚"),
                                             QStringLiteral("高频"),
                                             QStringLiteral("10 秒以下"),
                                             QStringLiteral("1X"),
                                             QStringLiteral("经典波形"),
                                         }},
        };
        return table;
    }

    // 名称是否相同（忽略首尾空白与大小写）
    bool same_name(const QString& left, const QString& right) {
        return left.trimmed().compare(right.trimmed(), Qt::CaseInsensitive) == 0;
    }

    // 标签在列表中的下标（大小写不敏感），未找到返回 -1
    int index_of_tag(const QStringList& tags, const QString& tag) {
        for (int index = 0; index < tags.size(); ++index) {
            if (tags.at(index).compare(tag, Qt::CaseInsensitive) == 0) {
                return index;
            }
        }
        return -1;
    }

} // namespace

const std::vector<TagCategoryDef>& TagCatalog::builtin_categories() {
    return category_table();
}

QStringList TagCatalog::builtin_category_names() {
    QStringList names;
    for (const TagCategoryDef& category : builtin_categories()) {
        names.append(category.name);
    }
    return names;
}

int TagCatalog::builtin_category_count() {
    return static_cast<int>(builtin_categories().size());
}

bool TagCatalog::is_builtin_category(const QString& name) {
    if (name.trimmed().isEmpty()) {
        return false;
    }
    for (const TagCategoryDef& category : builtin_categories()) {
        if (same_name(category.name, name)) {
            return true;
        }
    }
    return false;
}

QString TagCatalog::user_category_name() {
    // 固定七类的最后一类；表结构变化时以表为准
    const std::vector<TagCategoryDef>& table = builtin_categories();
    return table.empty() ? QString() : table.back().name;
}

QStringList TagCatalog::builtin_tags() {
    QStringList result;
    for (const TagCategoryDef& category : builtin_categories()) {
        for (const QString& tag : category.tags) {
            const QString normalized = Wave::normalize_tag(tag);
            if (!normalized.isEmpty() && index_of_tag(result, normalized) < 0) {
                result.append(normalized);
            }
        }
    }
    return result;
}

bool TagCatalog::is_builtin_tag(const QString& tag) {
    const QString normalized = Wave::normalize_tag(tag);
    return !normalized.isEmpty() && index_of_tag(builtin_tags(), normalized) >= 0;
}

QString TagCatalog::category_of_builtin_tag(const QString& tag) {
    const QString normalized = Wave::normalize_tag(tag);
    if (normalized.isEmpty()) {
        return QString();
    }
    for (const TagCategoryDef& category : builtin_categories()) {
        for (const QString& existing : category.tags) {
            if (existing.compare(normalized, Qt::CaseInsensitive) == 0) {
                return category.name;
            }
        }
    }
    return QString();
}

QStringList TagCatalog::builtin_tags_of_official_wave(const QString& wave_name) {
    const QString name = wave_name.trimmed();
    if (name.isEmpty()) {
        return QStringList();
    }
    for (const auto& [key, tags] : official_wave_tag_table()) {
        if (!same_name(key, name)) {
            continue;
        }
        QStringList result;
        for (const QString& tag : tags) {
            const QString normalized = Wave::normalize_tag(tag);
            if (!normalized.isEmpty() && index_of_tag(result, normalized) < 0) {
                result.append(normalized);
            }
        }
        return result;
    }
    return QStringList();
}

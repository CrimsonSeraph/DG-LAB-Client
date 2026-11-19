#include "TagRule.h"

#include "Wave.h"

namespace {

    // 时长阈值（毫秒）
    constexpr int TEN_SECONDS = 10 * 1000;
    constexpr int ONE_MINUTE = 60 * 1000;
    constexpr int FIVE_MINUTES = 5 * 60 * 1000;

    // 频率字节语义：值大 = 周期长 = 低频，值小 = 周期短 = 高频；范围 10~240
    constexpr int PURE_HIGH_FREQ_AVG = 20;    // 纯高频：平均值极低
    constexpr int PURE_HIGH_FREQ_SPREAD = 10; // 纯高频：且变化极小（几乎恒定）
    constexpr int HIGH_FREQ_AVG = 50;         // 高频：平均值偏低
    constexpr int LOW_FREQ_AVG = 60;          // 低频：平均值偏高
    constexpr int MIXED_SPREAD = 80;          // 大幅变化，需结合平均值归边

    /// @brief 标签在列表中的下标（大小写不敏感）
    /// @param tags 标签列表
    /// @param tag 待查找的标签
    /// @return 标签下标；未找到返回 -1
    int index_of_tag(const QStringList& tags, const QString& tag) {
        for (int index = 0; index < tags.size(); ++index) {
            if (tags.at(index).compare(tag, Qt::CaseInsensitive) == 0) {
                return index;
            }
        }
        return -1;
    }

    /// @brief 按时长判定波形应挂的“波形时长”标签
    /// @param wave 波形
    /// @return 时长分类标签；无帧（仅 raw）时返回空列表
    QStringList duration_tags(const Wave& wave) {
        if (wave.frame_count() <= 0) {
            return {}; // 只有 raw、没有帧：无法判断时长，不挂标签
        }
        const int ms = wave.duration_ms();
        if (ms < TEN_SECONDS) return {QStringLiteral("10 秒以下")};
        if (ms < ONE_MINUTE) return {QStringLiteral("10 秒-1 分钟")};
        if (ms < FIVE_MINUTES) return {QStringLiteral("1-5 分钟")};
        return {QStringLiteral("5 分钟以上")};
    }

    /// @brief 按来源判定波形应挂的“波形来源”标签
    /// @param wave 波形
    /// @return 经典波形（官方）/ 自定义波形（用户）
    QStringList source_tags(const Wave& wave) {
        return {wave.is_official() ? QStringLiteral("经典波形")
                                   : QStringLiteral("自定义波形")};
    }

    // 从 V3 帧解析频率统计（每帧 8 字节：前 4 字节频率，后 4 字节强度）
    struct FreqStats {
        int min_freq = 255;
        int max_freq = 0;
        double avg_freq = 0.0;
        int samples = 0;
    };

    /// @brief 统计 V3 帧中的频率分布（每帧前 4 字节为频率）
    /// @param wave 波形
    /// @return 频率最小值、最大值、平均值与样本数；无可用帧时样本数为 0
    FreqStats analyze_frequencies(const Wave& wave) {
        FreqStats stats;
        if (wave.frames_v3.isEmpty()) {
            return stats;
        }
        long long sum = 0;
        for (const QString& frame : wave.frames_v3) {
            if (frame.size() != 16) continue;
            for (int unit = 0; unit < 4; ++unit) {
                bool ok = false;
                const int freq = frame.mid(unit * 2, 2).toInt(&ok, 16);
                if (!ok) continue;
                stats.min_freq = std::min(stats.min_freq, freq);
                stats.max_freq = std::max(stats.max_freq, freq);
                sum += freq;
                ++stats.samples;
            }
        }
        if (stats.samples > 0) {
            stats.avg_freq = static_cast<double>(sum) / stats.samples;
        }
        return stats;
    }

    /// @brief 按频率分布判定波形应挂的“频率风格”标签
    /// @param wave 波形
    /// @return 纯高频 / 高频 / 低频 / 混合；无可用帧时返回空列表
    QStringList frequency_style_tags(const Wave& wave) {
        const FreqStats stats = analyze_frequencies(wave);
        if (stats.samples == 0) {
            return {};
        }
        const int range = stats.max_freq - stats.min_freq;

        // 纯高频：频率字节极低（周期最短）且几乎恒定
        if (stats.avg_freq <= PURE_HIGH_FREQ_AVG && range <= PURE_HIGH_FREQ_SPREAD) {
            return {QStringLiteral("纯高频")};
        }
        // 大幅变化：平均值偏高归低频（信号灯、变速敲击），否则归混合（心跳节奏、波浪涟漪）
        if (range >= MIXED_SPREAD) {
            if (stats.avg_freq >= LOW_FREQ_AVG) {
                return {QStringLiteral("低频")};
            }
            return {QStringLiteral("混合")};
        }
        // 小幅变化：按平均值分档
        if (stats.avg_freq <= HIGH_FREQ_AVG) {
            return {QStringLiteral("高频")};
        }
        if (stats.avg_freq >= LOW_FREQ_AVG) {
            return {QStringLiteral("低频")};
        }
        return {QStringLiteral("混合")}; // 中间态
    }

} // namespace

QStringList TagRule::auto_tags_for(const Wave& wave) {
    QStringList result;
    const auto append = [&result](const QStringList& tags) {
        for (const QString& tag : tags) {
            if (index_of_tag(result, tag) < 0) {
                result.append(tag);
            }
        }
    };
    append(duration_tags(wave));
    append(frequency_style_tags(wave));
    append(source_tags(wave));
    return Wave::normalize_tags(result);
}

bool TagRule::is_auto_tag(const QString& tag) {
    // 只列出由规则产出的标签名，用于界面徽标 / 日志
    static const QStringList kAutoTags = {
        QStringLiteral("10 秒以下"),
        QStringLiteral("10 秒-1 分钟"),
        QStringLiteral("1-5 分钟"),
        QStringLiteral("5 分钟以上"),
        QStringLiteral("纯高频"),
        QStringLiteral("高频"),
        QStringLiteral("低频"),
        QStringLiteral("混合"),
        QStringLiteral("经典波形"),
        QStringLiteral("自定义波形"),
    };
    return kAutoTags.contains(tag, Qt::CaseInsensitive);
}

#pragma once

#include <QString>

namespace DGLABClientUtil {
    inline static bool contains_any_keyword(const QString& str, const std::vector<std::string>& keywords) {
        QString lowerStr = str.toLower();
        for (const auto& kw : keywords) {
            if (lowerStr.contains(QString::fromStdString(kw))) {
                return true;
            }
        }
        return false;
    }
}

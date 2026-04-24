#include "DebugLog.h"
#include "IpSelector.h"

#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QDebug>

IpSelector* IpSelector::instance_ = nullptr;

namespace {
    class IpSelectionDialog : public QDialog {
        Q_OBJECT

    public:
        explicit IpSelectionDialog(const QStringList& current_blacklist,
            const QStringList& current_whitelist,
            QWidget* parent = nullptr);

        QString selected_ip() const { return selected_ip_; }
        QStringList blacklist() const { return blacklist_; }
        QStringList whitelist() const { return whitelist_; }

    private slots:
        void on_blacklist_add();
        void on_blacklist_remove();
        void on_whitelist_add();
        void on_whitelist_remove();
        void refresh_ip_list();
        void on_ip_selection_changed();
        void on_auto_select();
        void on_accept();

    private:
        void setup_ui();
        QStringList get_filtered_ips() const;

        QStringList blacklist_;
        QStringList whitelist_;
        QString selected_ip_;

        QListWidget* blacklist_widget_;
        QLineEdit* blacklist_input_;
        QListWidget* whitelist_widget_;
        QLineEdit* whitelist_input_;
        QListWidget* ip_list_widget_;
        QPushButton* auto_select_btn_;
        QPushButton* ok_btn_;
        QPushButton* cancel_btn_;
    };

    IpSelectionDialog::IpSelectionDialog(const QStringList& current_blacklist,
        const QStringList& current_whitelist,
        QWidget* parent)
        : QDialog(parent)
        , blacklist_(current_blacklist)
        , whitelist_(current_whitelist) {
        LOG_MODULE("IpSelector", "IpSelectionDialog", LOG_DEBUG, "对话框构造开始");
        setup_ui();
        refresh_ip_list();
        setWindowTitle("IP 选择器");
        resize(600, 400);
        LOG_MODULE("IpSelector", "IpSelectionDialog", LOG_DEBUG, "对话框构造完成");
    }

    void IpSelectionDialog::setup_ui() {
        LOG_MODULE("IpSelector", "setup_ui", LOG_DEBUG, "初始化UI");
        QVBoxLayout* main_layout = new QVBoxLayout(this);
        QHBoxLayout* lists_layout = new QHBoxLayout;

        // 黑名单区域
        QVBoxLayout* black_layout = new QVBoxLayout;
        black_layout->addWidget(new QLabel("黑名单关键词（网卡名包含则忽略）:"));
        blacklist_widget_ = new QListWidget;
        blacklist_widget_->addItems(blacklist_);
        black_layout->addWidget(blacklist_widget_);

        QHBoxLayout* black_btn_layout = new QHBoxLayout;
        blacklist_input_ = new QLineEdit;
        blacklist_input_->setPlaceholderText("输入关键词");
        QPushButton* add_black_btn = new QPushButton("添加");
        QPushButton* remove_black_btn = new QPushButton("删除选中");
        connect(add_black_btn, &QPushButton::clicked, this, &IpSelectionDialog::on_blacklist_add);
        connect(remove_black_btn, &QPushButton::clicked, this, &IpSelectionDialog::on_blacklist_remove);
        black_btn_layout->addWidget(blacklist_input_);
        black_btn_layout->addWidget(add_black_btn);
        black_btn_layout->addWidget(remove_black_btn);
        black_layout->addLayout(black_btn_layout);
        lists_layout->addLayout(black_layout);

        // 白名单区域
        QVBoxLayout* white_layout = new QVBoxLayout;
        white_layout->addWidget(new QLabel("白名单关键词（匹配则优先返回）:"));
        whitelist_widget_ = new QListWidget;
        whitelist_widget_->addItems(whitelist_);
        white_layout->addWidget(whitelist_widget_);

        QHBoxLayout* white_btn_layout = new QHBoxLayout;
        whitelist_input_ = new QLineEdit;
        whitelist_input_->setPlaceholderText("输入关键词");
        QPushButton* add_white_btn = new QPushButton("添加");
        QPushButton* remove_white_btn = new QPushButton("删除选中");
        connect(add_white_btn, &QPushButton::clicked, this, &IpSelectionDialog::on_whitelist_add);
        connect(remove_white_btn, &QPushButton::clicked, this, &IpSelectionDialog::on_whitelist_remove);
        white_btn_layout->addWidget(whitelist_input_);
        white_btn_layout->addWidget(add_white_btn);
        white_btn_layout->addWidget(remove_white_btn);
        white_layout->addLayout(white_btn_layout);
        lists_layout->addLayout(white_layout);

        main_layout->addLayout(lists_layout);

        // 可用 IP 列表区域
        main_layout->addWidget(new QLabel("当前有效的 IP 地址（根据黑白名单过滤）:"));
        ip_list_widget_ = new QListWidget;
        ip_list_widget_->setSelectionMode(QAbstractItemView::SingleSelection);
        connect(ip_list_widget_, &QListWidget::itemSelectionChanged,
            this, &IpSelectionDialog::on_ip_selection_changed);
        main_layout->addWidget(ip_list_widget_);

        // 按钮区域
        QHBoxLayout* button_layout = new QHBoxLayout;
        auto_select_btn_ = new QPushButton("自动匹配并选择");
        ok_btn_ = new QPushButton("确定");
        cancel_btn_ = new QPushButton("取消");
        connect(auto_select_btn_, &QPushButton::clicked, this, &IpSelectionDialog::on_auto_select);
        connect(ok_btn_, &QPushButton::clicked, this, &IpSelectionDialog::on_accept);
        connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
        button_layout->addWidget(auto_select_btn_);
        button_layout->addStretch();
        button_layout->addWidget(ok_btn_);
        button_layout->addWidget(cancel_btn_);
        main_layout->addLayout(button_layout);
        LOG_MODULE("IpSelector", "setup_ui", LOG_DEBUG, "UI初始化完成");
    }

    void IpSelectionDialog::on_blacklist_add() {
        LOG_MODULE("IpSelector", "on_blacklist_add", LOG_DEBUG, "添加黑名单关键词");
        QString keyword = blacklist_input_->text().trimmed();
        if (keyword.isEmpty()) {
            QMessageBox::warning(this, "提示", "关键词不能为空");
            return;
        }
        if (!blacklist_.contains(keyword)) {
            blacklist_.append(keyword);
            blacklist_widget_->addItem(keyword);
            refresh_ip_list();
        }
        blacklist_input_->clear();
    }

    void IpSelectionDialog::on_blacklist_remove() {
        LOG_MODULE("IpSelector", "on_blacklist_remove", LOG_DEBUG, "删除黑名单关键词");
        QListWidgetItem* item = blacklist_widget_->currentItem();
        if (item) {
            QString keyword = item->text();
            blacklist_.removeAll(keyword);
            delete item;
            refresh_ip_list();
        }
    }

    void IpSelectionDialog::on_whitelist_add() {
        LOG_MODULE("IpSelector", "on_whitelist_add", LOG_DEBUG, "添加白名单关键词");
        QString keyword = whitelist_input_->text().trimmed();
        if (keyword.isEmpty()) {
            QMessageBox::warning(this, "提示", "关键词不能为空");
            return;
        }
        if (!whitelist_.contains(keyword)) {
            whitelist_.append(keyword);
            whitelist_widget_->addItem(keyword);
            refresh_ip_list();
        }
        whitelist_input_->clear();
    }

    void IpSelectionDialog::on_whitelist_remove() {
        LOG_MODULE("IpSelector", "on_whitelist_remove", LOG_DEBUG, "删除白名单关键词");
        QListWidgetItem* item = whitelist_widget_->currentItem();
        if (item) {
            QString keyword = item->text();
            whitelist_.removeAll(keyword);
            delete item;
            refresh_ip_list();
        }
    }

    QStringList IpSelectionDialog::get_filtered_ips() const {
        LOG_MODULE("IpSelector", "get_filtered_ips", LOG_DEBUG, "开始获取过滤后的IP列表");
        QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        QStringList valid_ips;

        for (const QNetworkInterface& iface : interfaces) {
            if (!(iface.flags() & QNetworkInterface::IsUp) ||
                (iface.flags() & QNetworkInterface::IsLoopBack)) {
                continue;
            }

            QString iface_name = iface.name();
            QString iface_human = iface.humanReadableName();

            // 黑名单过滤
            if (IpSelector::contains_any_keyword(iface_name, blacklist_) ||
                IpSelector::contains_any_keyword(iface_human, blacklist_)) {
                continue;
            }

            // 获取 IPv4
            QString ipv4;
            for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
                QHostAddress ip = entry.ip();
                if (ip.protocol() == QAbstractSocket::IPv4Protocol && ip != QHostAddress::LocalHost) {
                    ipv4 = ip.toString();
                    break;
                }
            }
            if (!ipv4.isEmpty()) {
                valid_ips.append(ipv4);
            }
        }
        QString msg = QString("找到 %1 个有效IP").arg(valid_ips.size());
        LOG_MODULE("IpSelector", "get_filtered_ips", LOG_DEBUG, msg.toStdString());
        return valid_ips;
    }

    void IpSelectionDialog::refresh_ip_list() {
        LOG_MODULE("IpSelector", "refresh_ip_list", LOG_DEBUG, "刷新IP列表");
        ip_list_widget_->clear();
        QStringList ips = get_filtered_ips();
        ip_list_widget_->addItems(ips);
        if (ips.isEmpty()) {
            ip_list_widget_->addItem("（无可用 IP）");
            ok_btn_->setEnabled(false);
            LOG_MODULE("IpSelector", "refresh_ip_list", LOG_WARN, "无可用IP");
        }
        else {
            ok_btn_->setEnabled(true);
            QString msg = QString("列表刷新完成，共%1个IP").arg(ips.size());
            LOG_MODULE("IpSelector", "refresh_ip_list", LOG_DEBUG, msg.toStdString());
        }
        // 记录当前选中的IP（刷新后可能丢失选中）
        QListWidgetItem* current = ip_list_widget_->currentItem();
        if (current) {
            QString msg = QString("当前选中项: %1").arg(current->text());
            LOG_MODULE("IpSelector", "refresh_ip_list", LOG_DEBUG, msg.toStdString());
        }
        else {
            LOG_MODULE("IpSelector", "refresh_ip_list", LOG_DEBUG, "当前无选中项");
        }
    }

    void IpSelectionDialog::on_ip_selection_changed() {
        QListWidgetItem* item = ip_list_widget_->currentItem();
        if (item && item->text() != "（无可用 IP）") {
            selected_ip_ = item->text();
            LOG_MODULE("IpSelector", "on_ip_selection_changed", LOG_INFO, "用户手动选中IP: " + selected_ip_.toStdString());
        }
        else {
            selected_ip_.clear();
            LOG_MODULE("IpSelector", "on_ip_selection_changed", LOG_DEBUG, "选中项被清除");
        }
    }

    void IpSelectionDialog::on_auto_select() {
        LOG_MODULE("IpSelector", "on_auto_select", LOG_INFO, "用户点击自动匹配按钮");
        QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        QString fallback_ip;
        for (const QNetworkInterface& iface : interfaces) {
            if (!(iface.flags() & QNetworkInterface::IsUp) ||
                (iface.flags() & QNetworkInterface::IsLoopBack)) {
                continue;
            }

            QString iface_name = iface.name();
            QString iface_human = iface.humanReadableName();

            if (IpSelector::contains_any_keyword(iface_name, blacklist_) ||
                IpSelector::contains_any_keyword(iface_human, blacklist_)) {
                continue;
            }

            QString ipv4;
            for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
                QHostAddress ip = entry.ip();
                if (ip.protocol() == QAbstractSocket::IPv4Protocol && ip != QHostAddress::LocalHost) {
                    ipv4 = ip.toString();
                    break;
                }
            }
            if (ipv4.isEmpty()) continue;

            if (fallback_ip.isEmpty()) {
                fallback_ip = ipv4;
            }

            if (IpSelector::contains_any_keyword(iface_name, whitelist_) ||
                IpSelector::contains_any_keyword(iface_human, whitelist_)) {
                selected_ip_ = ipv4;
                LOG_MODULE("IpSelector", "on_auto_select", LOG_INFO, "自动匹配到白名单IP: " + ipv4.toStdString());
                QMessageBox::information(this, "自动匹配", QString("自动匹配到 IP: %1").arg(ipv4));
                accept();
                return;
            }
        }

        if (!fallback_ip.isEmpty()) {
            selected_ip_ = fallback_ip;
            LOG_MODULE("IpSelector", "on_auto_select", LOG_INFO, "未匹配到白名单，使用默认IP: " + fallback_ip.toStdString());
            QMessageBox::information(this, "自动匹配", QString("未匹配到白名单，使用默认 IP: %1").arg(fallback_ip));
            accept();
        }
        else {
            LOG_MODULE("IpSelector", "on_auto_select", LOG_ERROR, "没有找到任何有效IP");
            QMessageBox::warning(this, "自动匹配", "没有找到任何有效的 IP 地址");
            reject();
        }
    }

    void IpSelectionDialog::on_accept() {
        QListWidgetItem* current = ip_list_widget_->currentItem();
        LOG_MODULE("IpSelector", "on_accept", LOG_INFO, "用户点击确定按钮");
        if (current && current->text() != "（无可用 IP）") {
            QString chosen = current->text();
            selected_ip_ = chosen;
            LOG_MODULE("IpSelector", "on_accept", LOG_INFO, "确定时选中的IP: " + chosen.toStdString() + "（来自列表当前项）");
            accept();
        }
        else {
            LOG_MODULE("IpSelector", "on_accept", LOG_WARN, "确定时没有有效选中项");
            QMessageBox::warning(this, "提示", "请先选择一个 IP 地址");
        }
    }
}

IpSelector* IpSelector::instance() {
    if (!instance_) {
        instance_ = new IpSelector();
        LOG_MODULE("IpSelector", "instance", LOG_INFO, "单例实例创建");
    }
    return instance_;
}

QString IpSelector::auto_select_ip() {
    LOG_MODULE("IpSelector", "auto_select_ip", LOG_DEBUG, "自动选择IP开始");
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    QString fallback_ip;

    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        if (is_interface_filtered(iface)) {
            QString iface_human = iface.humanReadableName();
            continue;
        }

        QString ipv4 = get_ipv4_from_interface(iface);
        if (ipv4.isEmpty()) continue;

        if (fallback_ip.isEmpty()) {
            fallback_ip = ipv4;
        }

        QString iface_name = iface.name();
        QString iface_human = iface.humanReadableName();
        if (contains_any_keyword(iface_name, whitelist_) ||
            contains_any_keyword(iface_human, whitelist_)) {
            ip_cache_ = ipv4;
            LOG_MODULE("IpSelector", "auto_select_ip", LOG_INFO, "自动选择IP结果(白名单): " + ipv4.toStdString());
            return ipv4;
        }
    }

    if (!fallback_ip.isEmpty()) {
        ip_cache_ = fallback_ip;
        LOG_MODULE("IpSelector", "auto_select_ip", LOG_INFO, "自动选择IP结果(默认): " + fallback_ip.toStdString());
        return fallback_ip;
    }

    ip_cache_ = "127.0.0.1";
    LOG_MODULE("IpSelector", "auto_select_ip", LOG_WARN, "无有效IP，使用127.0.0.1");
    return "127.0.0.1";
}

QString IpSelector::show_selection_dialog(QWidget* parent) {
    LOG_MODULE("IpSelector", "show_selection_dialog", LOG_DEBUG, "弹出IP选择对话框");
    IpSelectionDialog dialog(blacklist_, whitelist_, parent);
    if (dialog.exec() == QDialog::Accepted) {
        // 更新本地的黑白名单为用户编辑后的版本
        set_blacklist(dialog.blacklist());
        set_whitelist(dialog.whitelist());
        QString selected = dialog.selected_ip();
        if (!selected.isEmpty()) {
            ip_cache_ = selected;
        }
        LOG_MODULE("IpSelector", "show_selection_dialog", LOG_INFO, "对话框返回IP: " + selected.toStdString());
        return selected;
    }
    LOG_MODULE("IpSelector", "show_selection_dialog", LOG_INFO, "对话框取消，返回空字符串");
    return QString();
}

QStringList IpSelector::get_blacklist() const {
    return blacklist_;
}

void IpSelector::set_blacklist(const QStringList& list) {
    QString msg = QString("设置黑名单: %1").arg(list.join(","));
    LOG_MODULE("IpSelector", "set_blacklist", LOG_DEBUG, msg.toStdString());
    blacklist_ = list;
}

QStringList IpSelector::get_whitelist() const {
    return whitelist_;
}

void IpSelector::set_whitelist(const QStringList& list) {
    QString msg = QString("设置白名单: %1").arg(list.join(","));
    LOG_MODULE("IpSelector", "set_whitelist", LOG_DEBUG, msg.toStdString());
    whitelist_ = list;
}

bool IpSelector::contains_any_keyword(const QString& str, const QStringList& keywords) {
    for (const QString& kw : keywords) {
        if (str.contains(kw, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

IpSelector::IpSelector(QObject* parent)
    : QObject(parent) {
    blacklist_ << "vmware" << "virtual" << "docker" << "vbox";
    whitelist_ << "以太网" << "wlan" << "en0" << "eth";
    ip_cache_ = "127.0.0.1";
    LOG_MODULE("IpSelector", "IpSelector", LOG_DEBUG, "IpSelector对象构造完成，默认黑白名单已设置");
}

QStringList IpSelector::get_all_valid_ips() const {
    LOG_MODULE("IpSelector", "get_all_valid_ips", LOG_DEBUG, "获取所有有效IP");
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    QStringList result;

    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        if (is_interface_filtered(iface)) {
            continue;
        }

        QString ipv4 = get_ipv4_from_interface(iface);
        if (!ipv4.isEmpty()) {
            result.append(ipv4);
        }
    }
    QString msg = QString("找到%1个有效IP").arg(result.size());
    LOG_MODULE("IpSelector", "get_all_valid_ips", LOG_DEBUG, msg.toStdString());
    return result;
}

bool IpSelector::is_interface_filtered(const QNetworkInterface& iface) const {
    QString iface_name = iface.name();
    QString iface_human = iface.humanReadableName();
    bool filtered = contains_any_keyword(iface_name, blacklist_) ||
        contains_any_keyword(iface_human, blacklist_);
    if (filtered) {
        QString msg = QString("接口被过滤: %1 (%2)").arg(iface_human, iface_name);
        LOG_MODULE("IpSelector", "is_interface_filtered", LOG_DEBUG, msg.toStdString());
    }
    return filtered;
}

QString IpSelector::get_ipv4_from_interface(const QNetworkInterface& iface) const {
    for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
        QHostAddress ip = entry.ip();
        if (ip.protocol() == QAbstractSocket::IPv4Protocol && ip != QHostAddress::LocalHost) {
            return ip.toString();
        }
    }
    return QString();
}

#include "IpSelector.moc"

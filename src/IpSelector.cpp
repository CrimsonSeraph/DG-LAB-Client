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
        setup_ui();
        refresh_ip_list();
        setWindowTitle("IP 选择器");
        resize(600, 400);
    }

    void IpSelectionDialog::setup_ui() {
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
    }

    void IpSelectionDialog::on_blacklist_add() {
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
        QListWidgetItem* item = blacklist_widget_->currentItem();
        if (item) {
            QString keyword = item->text();
            blacklist_.removeAll(keyword);
            delete item;
            refresh_ip_list();
        }
    }

    void IpSelectionDialog::on_whitelist_add() {
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
        QListWidgetItem* item = whitelist_widget_->currentItem();
        if (item) {
            QString keyword = item->text();
            whitelist_.removeAll(keyword);
            delete item;
            refresh_ip_list();
        }
    }

    QStringList IpSelectionDialog::get_filtered_ips() const {
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
        return valid_ips;
    }

    void IpSelectionDialog::refresh_ip_list() {
        ip_list_widget_->clear();
        QStringList ips = get_filtered_ips();
        ip_list_widget_->addItems(ips);
        if (ips.isEmpty()) {
            ip_list_widget_->addItem("（无可用 IP）");
            ok_btn_->setEnabled(false);
        }
        else {
            ok_btn_->setEnabled(true);
        }
    }

    void IpSelectionDialog::on_ip_selection_changed() {
        QListWidgetItem* item = ip_list_widget_->currentItem();
        if (item && item->text() != "（无可用 IP）") {
            selected_ip_ = item->text();
        }
        else {
            selected_ip_.clear();
        }
    }

    void IpSelectionDialog::on_auto_select() {
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
                QMessageBox::information(this, "自动匹配", QString("自动匹配到 IP：%1").arg(ipv4));
                accept();
                return;
            }
        }

        if (!fallback_ip.isEmpty()) {
            selected_ip_ = fallback_ip;
            QMessageBox::information(this, "自动匹配", QString("未匹配到白名单，使用默认 IP：%1").arg(fallback_ip));
            accept();
        }
        else {
            QMessageBox::warning(this, "自动匹配", "没有找到任何有效的 IP 地址");
            reject();
        }
    }

    void IpSelectionDialog::on_accept() {
        if (selected_ip_.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先选择一个 IP 地址");
            return;
        }
        accept();
    }

}

IpSelector* IpSelector::instance() {
    if (!instance_) {
        instance_ = new IpSelector();
    }
    return instance_;
}

QString IpSelector::auto_select_ip() {
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    QString fallback_ip;

    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        if (is_interface_filtered(iface)) {
            QString iface_human = iface.humanReadableName();
            qDebug() << "黑名单过滤网卡:" << iface_human;
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
            qDebug() << "白名单匹配网卡:" << iface_human << " IP:" << ipv4;
            ip_cache_ = ipv4;
            return ipv4;
        }
    }

    if (!fallback_ip.isEmpty()) {
        ip_cache_ = fallback_ip;
        return fallback_ip;
    }

    ip_cache_ = "127.0.0.1";
    return "127.0.0.1";
}

QString IpSelector::show_selection_dialog(QWidget* parent) {
    IpSelectionDialog dialog(blacklist_, whitelist_, parent);
    if (dialog.exec() == QDialog::Accepted) {
        // 更新本地的黑白名单为用户编辑后的版本
        set_blacklist(dialog.blacklist());
        set_whitelist(dialog.whitelist());
        QString selected = dialog.selected_ip();
        if (!selected.isEmpty()) {
            ip_cache_ = selected;
        }
        return selected;
    }
    return QString();
}

QStringList IpSelector::get_blacklist() const {
    return blacklist_;
}

void IpSelector::set_blacklist(const QStringList& list) {
    blacklist_ = list;
}

QStringList IpSelector::get_whitelist() const {
    return whitelist_;
}

void IpSelector::set_whitelist(const QStringList& list) {
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
}

QStringList IpSelector::get_all_valid_ips() const {
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
    return result;
}

bool IpSelector::is_interface_filtered(const QNetworkInterface& iface) const {
    QString iface_name = iface.name();
    QString iface_human = iface.humanReadableName();
    return contains_any_keyword(iface_name, blacklist_) ||
        contains_any_keyword(iface_human, blacklist_);
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

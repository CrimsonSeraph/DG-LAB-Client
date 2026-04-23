#pragma once

#include <QNetworkInterface>
#include <QObject>
#include <QStringList>

class IpSelector : public QObject
{
    Q_OBJECT

public:
    // 禁止拷贝和赋值
    IpSelector(const IpSelector&) = delete;
    IpSelector& operator=(const IpSelector&) = delete;

    /// @brief 获取单例实例
    static IpSelector* instance();

    /// @brief 自动获取 IP（根据当前黑白名单匹配）
    /// @return 匹配到的 IP 字符串，若无有效 IP 则返回 "127.0.0.1"
    QString auto_select_ip();

    /// @brief 弹出选择对话框，允许用户编辑黑白名单并手动选择 IP
    /// @param parent 父窗口
    /// @return 用户选择的 IP，若取消则返回空字符串
    QString show_selection_dialog(QWidget* parent = nullptr);

    /// @brief 获取当前黑名单关键词列表
    QStringList get_blacklist() const;

    /// @brief 设置黑名单关键词列表
    void set_blacklist(const QStringList& list);

    /// @brief 获取当前白名单关键词列表
    QStringList get_whitelist() const;

    /// @brief 设置白名单关键词列表
    void set_whitelist(const QStringList& list);

    /// @brief 检查字符串是否包含任意关键词（大小写不敏感）
    /// @param str 待检查字符串
    /// @param keywords 关键词列表
    /// @return 如果包含至少一个关键词则返回 true
    static bool contains_any_keyword(const QString& str, const QStringList& keywords);

private:
    static IpSelector* instance_;   ///< 单例实例指针
    QStringList blacklist_; ///< 黑名单关键词（网卡名称包含则忽略）
    QStringList whitelist_; ///< 白名单关键词（匹配则优先返回）
    QString ip_cache_;  ///< 上次获取的 IP 缓存

    explicit IpSelector(QObject* parent = nullptr);
    ~IpSelector() = default;

    /// @brief 获取所有符合当前过滤规则的有效 IPv4 地址
    /// @return 有效 IP 列表（按发现顺序）
    QStringList get_all_valid_ips() const;

    /// @brief 判断网络接口是否被黑名单过滤
    /// @param iface 网络接口
    /// @return true 表示应忽略该接口
    bool is_interface_filtered(const QNetworkInterface& iface) const;

    /// @brief 从网络接口中获取非回环的 IPv4 地址
    /// @param iface 网络接口
    /// @return IPv4 地址字符串，若无则返回空字符串
    QString get_ipv4_from_interface(const QNetworkInterface& iface) const;
};

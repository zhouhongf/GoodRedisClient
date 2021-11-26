#include "connectionsmanager.h"

#include <QAbstractItemModel>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include "app/events.h"
#include "configmanager.h"
#include "modules/bulk-operations/bulkoperationsmanager.h"
#include "modules/connections-tree/items/serveritem.h"
#include "modules/connections-tree/items/servergroup.h"
#include "modules/value-editor/tabsmodel.h"


ConnectionsManager::ConnectionsManager(const QString &configPath, QSharedPointer<Events> events) : ConnectionsTree::Model(), m_configPath(configPath), m_events(events) {
    connect(this, &ConnectionsTree::Model::error, m_events.data(), &Events::error);
}

ConnectionsManager::~ConnectionsManager(void) {}

void ConnectionsManager::loadConnections() {
    if (!m_configPath.isEmpty() && QFile::exists(m_configPath)) {
        loadConnectionsConfigFromFile(m_configPath);
    }
    emit connectionsLoaded();
}

void ConnectionsManager::addNewConnection(const ServerConfig &config, bool saveToConfig, QSharedPointer<ConnectionsTree::ServerGroup> group) {
    createServerItemForConnection(config, group);
    if (saveToConfig) {
        saveConfig();
    }
    buildConnectionsCache();
}

void ConnectionsManager::addNewGroup(const QString &name) {
    auto group = QSharedPointer<ConnectionsTree::ServerGroup>(new ConnectionsTree::ServerGroup(name, *static_cast<ConnectionsTree::Model *>(this)));
    addGroup(group);
    saveConfig();
}

void ConnectionsManager::updateGroup(const ConnectionGroup &group) {
    auto serverGroup = group.serverGroup();
    if (!serverGroup) {
        qWarning() << "invalid server group";
        return;
    }
    itemChanged(serverGroup);
    saveConfig();
    buildConnectionsCache();
}

void ConnectionsManager::updateConnection(const ServerConfig &config) {
    if (!config.owner()) {
        return addNewConnection(config);
    }
    auto treeOperations = config.owner().toStrongRef();
    if (!treeOperations) {
        return;
    }
    treeOperations->setConfig(config);
    saveConfig();
}

bool ConnectionsManager::importConnections(const QString &path) {
    if (loadConnectionsConfigFromFile(path, true)) {
        emit sizeChanged();
        return true;
    }
    return false;
}

// 从文件中读取 连接设置
bool ConnectionsManager::loadConnectionsConfigFromFile(const QString &config, bool saveChangesToFile) {
    QJsonArray connections;
    // 读取文件
    QFile conf(config);
    if (!conf.open(QIODevice::ReadOnly)) {
        return false;
    }
    QByteArray data = conf.readAll();
    conf.close();
    // 转换为json格式的内容
    QJsonDocument jsonConfig = QJsonDocument::fromJson(data);
    if (jsonConfig.isEmpty()) {
        return true;
    }
    if (!jsonConfig.isArray()) {
        return false;
    }
    qDebug() << "==== ConnectionsManager::loadConnectionsConfigFromFile ====: " << jsonConfig;
    // 取json的array()值，并遍历
    connections = jsonConfig.array();
    for (QJsonValue connection : connections) {
        if (!connection.isObject()) {
            continue;
        }
        auto obj = connection.toObject();
        // 如果 连接设置的信息 中 包含如下信息，则按照group组，来设置
        if (obj.contains("type") && obj.contains("connections") && obj.contains("name") && obj["connections"].isArray() && obj["type"].toString().toLower() == "group") {
            auto groupConnections = obj["connections"].toArray();
            qDebug() << "==== ConnectionsManager::loadConnectionsConfigFromFile groupConnections ====: " << groupConnections;
            auto group = QSharedPointer<ConnectionsTree::ServerGroup>(new ConnectionsTree::ServerGroup(obj["name"].toString(), *static_cast<ConnectionsTree::Model *>(this)));
            // 遍历groupConnections集合
            for (QJsonValue c : groupConnections) {
                if (!c.isObject()) {
                    continue;
                }
                ServerConfig conf(c.toObject().toVariantHash());
                if (conf.isNull()) {
                    continue;
                }
                // 设置ServerConfig的UUID，并调用addNewConnection方法
                conf.setId(QUuid::createUuid().toByteArray());
                addNewConnection(conf, false, group);
            }
            // 添加组
            addGroup(group);
        }

        // 设置ServerConfig，并调用addNewConnection方法
        ServerConfig conf(obj.toVariantHash());
        if (conf.isNull()) {
            continue;
        }
        addNewConnection(conf, false);
    }

    // 如果是保存到文件中，则执行保存
    if (saveChangesToFile) {
        saveConfig();
    }
    // 建设连接缓存
    buildConnectionsCache();
    return true;
}

void ConnectionsManager::saveConfig() {
    saveConnectionsConfigToFile(m_configPath);
}

// 保存 连接设置 到文件
bool ConnectionsManager::saveConnectionsConfigToFile(const QString &pathToFile) {
    qDebug() << "==== ConnectionsManager::saveConnectionsConfigToFile pathToFile ====: " << pathToFile;
    QJsonArray connections;
    auto addConfig = [](QSharedPointer<ConnectionsTree::TreeItem> i, QJsonArray & connections) {
        auto srvItem = i.dynamicCast<ConnectionsTree::ServerItem>();
        if (!srvItem) {
            return;
        }
        auto op = srvItem->getOperations().dynamicCast<TreeOperations>();
        if (!op) {
            return;
        }
        connections.push_back(QJsonValue(op->config().toJsonObject()));
    };
    // 遍历 m_treeItems，分别保存server_group 和 server
    for (auto item : m_treeItems) {
        if (item->type() == "server_group") {
            QJsonObject group;
            group["type"] = "group";
            group["name"] = item->getDisplayName();
            QJsonArray groupConnections;
            for (auto srv : item->getAllChilds()) {
                addConfig(srv, groupConnections);
            }
            group["connections"] = groupConnections;
            connections.push_back(QJsonValue(group));
        } else if (item->type() == "server") {
            addConfig(item, connections);
        }
    }
    return saveJsonArrayToFile(connections, pathToFile);
}

bool ConnectionsManager::testConnectionSettings(const ServerConfig &config) {
    RedisClient::Connection testConnection(config);
    // 注册log日志
    m_events->registerLoggerForConnection(testConnection);
    try {
        return testConnection.connect();
    } catch (const RedisClient::Connection::Exception &) {
        return false;
    }
}

ServerConfig ConnectionsManager::createEmptyConfig() const {
    return ServerConfig();
}

// 从连接地址中 解析 连接设置出来
// 分别设置host, port, username, auth, 或者 ssl
ServerConfig ConnectionsManager::parseConfigFromRedisConnectionString(const QString &connectionString) const {
    QUrl url = QUrl(connectionString);
    QUrlQuery query = QUrlQuery(url.query());
    ServerConfig config;
    config.setHost(url.host().isEmpty() || url.host() == "localhost" ? "127.0.0.1" : url.host());
    config.setPort(url.port() == -1 ? 6379 : url.port());
    config.setUsername(url.userName());
    config.setAuth(url.password().isEmpty() ? query.queryItemValue("password") : url.password());
    if (url.scheme() == "rediss" || (!query.isEmpty() && query.queryItemValue("ssl") == "true")) {
        config.setSsl(true);
    }
    return config;
}

// 验证 redis连接地址url 是否有效
bool ConnectionsManager::isRedisConnectionStringValid(const QString &connectionString) {
    QUrl url = QUrl(connectionString);
    return url.isValid() && (url.scheme() == "redis" || url.scheme() == "rediss") && !url.host().isEmpty();
}

// 有多少个连接
int ConnectionsManager::size() {
    int connectionsCount = 0;
    for (auto item : m_treeItems) {
        if (item->type() == "server_group") {
            connectionsCount += item->childCount();
        } else if (item->type() == "server") {
            connectionsCount++;
        }
    }
    return connectionsCount;
}

QSharedPointer<RedisClient::Connection> ConnectionsManager::getByIndex(int index) {
    auto op = m_connectionsCache.values().at(index)->getOperations();
    if (!op) {
        return QSharedPointer<RedisClient::Connection>();
    }
    auto treeOp = op.dynamicCast<TreeOperations>();
    if (!treeOp) {
        return QSharedPointer<RedisClient::Connection>();
    }
    return treeOp->connection();
}

QStringList ConnectionsManager::getConnections() {
    // 返回 QMap<QString, QSharedPointer<ConnectionsTree::ServerItem>>的keys
    return m_connectionsCache.keys();
}

void ConnectionsManager::applyGroupChanges() {
    ConnectionsTree::Model::applyGroupChanges();
    buildConnectionsCache();
    saveConfig();
}


// 创建 serverItem
void ConnectionsManager::createServerItemForConnection(const ServerConfig &config, QSharedPointer<ConnectionsTree::ServerGroup> group) {
    using namespace ConnectionsTree;
    auto treeModel = QSharedPointer<TreeOperations>(new TreeOperations(config, m_events));
    // 信号槽
    connect(treeModel.data(), &TreeOperations::createNewConnection, this, [this](const ServerConfig & config) {
        addNewConnection(config);
    });

    // 智能指针 弱引用
    QWeakPointer<TreeItem> parent;
    if (group) {
        parent = group.toWeakRef();
    }
    // 智能指针 强引用
    auto serverItem = QSharedPointer<ServerItem>(new ServerItem(treeModel.dynamicCast<ConnectionsTree::Operations>(), *static_cast<ConnectionsTree::Model *>(this), parent));
    serverItem->setWeakPointer(serverItem.toWeakRef());

    // 设置信号槽 ,函数4个参数：发射信号的对象，发射的信号，接受信号的对象，要执行的槽；
    // 设置信号槽 设置更新
    connect(treeModel.data(), &TreeOperations::configUpdated, this, [this, serverItem]() {
        if (!serverItem) {
            return;
        }
        itemChanged(serverItem.dynamicCast<ConnectionsTree::TreeItem>().toWeakRef());
    });
    // 设置信号槽 筛选历史更新
    connect(treeModel.data(), &TreeOperations::filterHistoryUpdated, this, [this]() {
        saveConfig();
    });
    // 设置信号槽 编辑行为
    connect(serverItem.data(), &ConnectionsTree::ServerItem::editActionRequested, this, [this, treeModel]() {
        if (!treeModel) {
            return;
        }
        emit connectionAboutToBeEdited(treeModel->config().name());
        emit editConnection(treeModel->config());
    });
    // 设置信号槽 删除行为
    connect(serverItem.data(), &ConnectionsTree::ServerItem::deleteActionRequested, this, [this, serverItem, treeModel, group]() {
        if (!serverItem || !treeModel) {
            return;
        }
        emit connectionAboutToBeEdited(treeModel->config().name());
        if (group) {
            group->removeChild(serverItem);
        } else {
            removeRootItem(serverItem);
        }
        buildConnectionsCache();
        emit sizeChanged();
        saveConfig();
    });
    // 添加serverItem
    if (group) {
        group->addServer(serverItem);
    } else {
        addRootItem(serverItem);
    }
}

void ConnectionsManager::addGroup(QSharedPointer<ConnectionsTree::ServerGroup> group) {
    // 槽函数，编辑行为
    connect(group.data(), &ConnectionsTree::ServerGroup::editActionRequested, this, [this, group]() {
        if (!group) {
            return;
        }
        ConnectionGroup g(group);
        emit editConnectionGroup(g);
    });
    // 槽函数，删除行为
    connect(group.data(), &ConnectionsTree::ServerGroup::deleteActionRequested, this, [this, group]() {
        if (!group) {
            return;
        }
        removeRootItem(group);
        buildConnectionsCache();
        emit sizeChanged();
        saveConfig();
    });
    // 添加根项目
    addRootItem(group);
    buildConnectionsCache();
}

void ConnectionsManager::buildConnectionsCache() {
    m_connectionsCache.clear();
    // 遍历 m_treeItems
    for (auto item : m_treeItems) {
        if (item->type() == "server_group") {
            QString nameTemplate = QString("[%1] %2").arg(item->getDisplayName());
            for (auto srv : item->getAllChilds()) {
                QString name = nameTemplate.arg(srv->getDisplayName());
                m_connectionsCache[name] = srv.dynamicCast<ConnectionsTree::ServerItem>();
            }
        } else if (item->type() == "server") {
            m_connectionsCache[item->getDisplayName()] = item.dynamicCast<ConnectionsTree::ServerItem>();
        }
    }
}

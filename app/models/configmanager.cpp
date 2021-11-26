#include "configmanager.h"
#include "compat.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantHash>
#include <QXmlStreamReader>
#include "app/models/serverconfig.h"


ConfigManager::ConfigManager(const QString &basePath) : m_basePath(basePath) {}

// 获取配置文件
QString ConfigManager::getApplicationConfigPath(const QString &configFile, bool checkPath) {
    QString configDir = getConfigPath(m_basePath);
    QDir settingsPath(configDir);
    // 如果不存在，则创建 目录
    if (!settingsPath.exists() && settingsPath.mkpath(configDir)) {
        qDebug() << "Config Dir created";
    }
    QString configPath = QString("%1/%2").arg(configDir).arg(configFile);
    if (checkPath && !chechPath(configPath)) {
        return QString();
    }
    return configPath;
}

// 获取设置目录路径
QString ConfigManager::getConfigPath(QString basePath) {
    QString configDir;
#ifdef Q_OS_MACX
    if (basePath == QDir::homePath()) {
        configDir = "/Library/Preferences/rdm/";
    } else {
        configDir = ".rdm";
    }
    configDir =
        QDir::toNativeSeparators(QString("%1/%2").arg(basePath).arg(configDir));
#else
    configDir =
        QDir::toNativeSeparators(QString("%1/%2").arg(basePath).arg(".rdm"));
#endif
    return configDir;
}




// 检查是否存在
bool ConfigManager::chechPath(const QString &configPath) {
    QFile testConfigFile(configPath);
    QFileInfo checkPermissionsFileInfo(configPath);
    // 如果文件不存在，或是只读的，则关闭文件
    if (!testConfigFile.exists() && testConfigFile.open(QIODevice::ReadWrite)) {
        testConfigFile.close();
    }
    // 如果文件可写，则设置权限
    if (checkPermissionsFileInfo.isWritable()) {
        setPermissions(testConfigFile);
        return true;
    }
    return false;
}

void ConfigManager::setPermissions(QFile &file) {
#ifdef Q_OS_WIN
    extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;
    qt_ntfs_permission_lookup++;
#endif
    if (!file.setPermissions(QFile::ReadUser | QFile::WriteUser)) {
        qWarning() << "Cannot set permissions for config folder";
    }
#ifdef Q_OS_WIN
    qt_ntfs_permission_lookup--;
#endif
}



// 将json保存至文件中
bool saveJsonArrayToFile(const QJsonArray &c, const QString &f) {
    QJsonDocument config(c);
    QFile confFile(f);
    if (confFile.open(QIODevice::WriteOnly)) {
        QTextStream outStream(&confFile);
        outStream.setCodec("UTF-8");
        outStream << config.toJson();
        confFile.close();
        return true;
    }
    return false;
}

#include "app.h"

#include "qpython.h"
#include "pythonlib_loader.h"
#include "redisclient.h"
#include <QMessageBox>
#include <QNetworkProxyFactory>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSettings>
#include <QSysInfo>
#include <QUrl>
#include <QtQml>
#include <QSslSocket>
#include <QtConcurrent>
#include <QTextCodec>

#if defined(Q_OS_WINDOWS) || defined(Q_OS_LINUX)
    #include "darkmode.h"
    #include <QStyleFactory>
#endif

#ifndef RDM_VERSION
    #define RDM_VERSION "2021.11.21"
#endif

#include "modules/common/tabviewmodel.h"
#include "events.h"
#include "models/configmanager.h"
#include "models/serverconfig.h"
#include "models/connectionsmanager.h"
#include "models/key-models/keyfactory.h"
#include "modules/bulk-operations/bulkoperationsmanager.h"
#include "modules/common/sortfilterproxymodel.h"
#include "modules/console/autocompletemodel.h"
#include "modules/console/consolemodel.h"
#include "modules/server-stats/serverstatsmodel.h"
#include "modules/value-editor/embeddedformattersmanager.h"
#ifdef ENABLE_EXTERNAL_FORMATTERS
    #include "modules/value-editor/externalformattersmanager.h"
#endif
#include "modules/value-editor/syntaxhighlighter.h"
#include "modules/value-editor/textcharformat.h"
#include "modules/value-editor/tabsmodel.h"
#include "modules/value-editor/valueviewmodel.h"
#include "qmlutils.h"


Application::Application(int &argc, char **argv)
    : QApplication(argc, argv),
      m_engine(this),
      m_qmlUtils(QSharedPointer<QmlUtils>(new QmlUtils())),
      m_events(QSharedPointer<Events>(new Events())) {
    // Init components required for models and qml
    initAppInfo();
    initProxySettings();
    processCmdArgs();
    initAppFonts();
#if defined(Q_OS_WINDOWS) || defined(Q_OS_LINUX)
    if (isDarkThemeEnabled()) {
        setStyle(QStyleFactory::create("Fusion"));
        //  setPalette(createDarkModePalette());
    }
#endif
    initRedisClient();
    installTranslator();
    initPython();
    qDebug() << QString("================= Application start =======================");
}

Application::~Application() {
    m_connections.clear();
}

void Application::initAppInfo() {
    setApplicationName("DataOffice");
    setApplicationVersion(QString(RDM_VERSION));
    setOrganizationDomain("jingrongbank.com");
    setOrganizationName("jingrongbank");
    setWindowIcon(QIcon(":/images/logo.png"));
    qDebug() << "TLS support:" << QSslSocket::sslLibraryVersionString();
}

void Application::initProxySettings() {
    QSettings settings;
    QNetworkProxyFactory::setUseSystemConfiguration(settings.value("app/useSystemProxy", false).toBool());
}


// 写程序时经常需要命令行解析，qt提供了 QCommandLineParser和QCommandLineOption两个类帮助快速解析命令行。
// QCommandLineOption mAnalysis("m", "自定义命令");
// 建立一个自定义命令   参数为m  即程序执行时输入-m则进入自己定义的命令
/*QCommandLineOption(const QString &name, const QString &description, const QString &valueName = QString(), const QString &defaultValue = QString())，
name参数表示选项的名称，
description参数表示选项的描述信息，输入-h后可以看到
valueName表示选项的取值的名称，
defaultValue表示选项的默认值*/
void Application::processCmdArgs() {
    QCommandLineParser parser;  // 建立命令行解析
    QCommandLineOption settingsDir("settings-dir",
                                   "(Optional) Directory where RDM looks/saves .rdm directory with connections.json file",
                                   "settingsDir",
                                   QDir::homePath());
    QCommandLineOption formattersDir("formatters-dir",
                                     "(Optional) Directory where RDM looks for native value formatters",
                                     "formattersDir",
                                 #ifdef Q_OS_WIN32
                                     QString("%1/formatters").arg(QCoreApplication::applicationDirPath()));
                                 #elif defined Q_OS_MACOS
                                     QString("%1/.rdm/formatters").arg(QDir::homePath()));
                                 #else
                                     QString());
                                 #endif
    QCommandLineOption renderingBackend("rendering-backend",
                                        "(Optional) QML rendering backend [software|opengl|d3d12|'']",
                                        "renderingBackend",
                                        "auto");

    parser.addHelpOption();             //增加-h/-help解析命令
    parser.addVersionOption();          //增加-v 解析命令
    parser.addOption(settingsDir);      //命令解析增加自定义命令
    parser.addOption(formattersDir);
    parser.addOption(renderingBackend);
    parser.process(*this);              //命令执行


    // 设置Application类的几个变量值
    m_settingsDir = parser.value(settingsDir);
    m_formattersDir = parser.value(formattersDir);
    m_renderingBackend = parser.value(renderingBackend);
    qDebug() << "m_settingsDir:" << m_settingsDir;
    qDebug() << "m_formattersDir:" << m_formattersDir;
    qDebug() << "m_renderingBackend:" << m_renderingBackend;
}


// 根据不同的系统，定义不同的字体，及字体大小
void Application::initAppFonts() {
    QSettings settings;
#ifdef Q_OS_MAC
    QString defaultFontName("Helvetica Neue");
    QString defaultMonospacedFont("Monaco");
    int defaultFontSize = 12;
#elif defined(Q_OS_WINDOWS)
    QString defaultFontName("Segoe UI");
    QString defaultMonospacedFont("Consolas");
    int defaultFontSize = 11;
#else
    QString defaultFontName("Open Sans");
    QString defaultMonospacedFont("Ubuntu Mono");
    int defaultFontSize = 11;
#endif
    int defaultValueSizeLimit = 150000;
    QString appFont = settings.value("app/appFont", defaultFontName).toString();
    if (appFont.isEmpty()) {
        appFont = defaultFontName;
    }
    int appFontSize = settings.value("app/appFontSize", defaultFontSize).toInt();
    if (appFontSize < 5) {
        appFontSize = defaultFontSize;
    }
    if (appFont == "Open Sans") {
#if defined(Q_OS_LINUX)
        int result = QFontDatabase::addApplicationFont("://fonts/OpenSans.ttc");
        if (result == -1) {
            appFont = "Ubuntu";
        }
#elif defined (Q_OS_WINDOWS)
        appFont = defaultFontName;
#endif
    }

    QString valuesFont = settings.value("app/valueEditorFont", defaultMonospacedFont).toString();
    if (valuesFont.isEmpty()) {
        valuesFont = defaultMonospacedFont;
    }
    int valuesFontSize = settings.value("app/valueEditorFontSize", defaultFontSize).toInt();
    if (valuesFontSize < 5) {
        valuesFontSize = defaultFontSize;
    }
    int valueSizeLimit = settings.value("app/valueSizeLimit", defaultValueSizeLimit).toInt();
    if (valueSizeLimit < 1000) {
        valueSizeLimit = defaultValueSizeLimit;
    }

    settings.setValue("app/appFont", appFont);
    settings.setValue("app/appFontSize", appFontSize);
    settings.setValue("app/valueEditorFont", valuesFont);
    settings.setValue("app/valueEditorFontSize", valuesFontSize);
    settings.setValue("app/valueSizeLimit", valueSizeLimit);

    // 设置字体和大小
    QFont defaultFont(appFont, appFontSize);
    QApplication::setFont(defaultFont);
    qDebug() << "App font:" << appFont << appFontSize;
    qDebug() << "Values font:" << valuesFont << valuesFontSize << valueSizeLimit;
}

// 初始化软件语言
void Application::installTranslator() {
    QSettings settings;
    QString preferredLocale = settings.value("app/locale", "system").toString();
    QString locale;
    if (preferredLocale == "system") {
        settings.setValue("app/locale", "system");
        locale = QLocale::system().uiLanguages().first().replace("-", "_");
        qDebug() << QLocale::system().uiLanguages();
        if (locale.isEmpty() || locale == "C") {
            locale = "en_US";
        }
        qDebug() << "Detected locale:" << locale;
    } else {
        locale = preferredLocale;
    }
    locale = "en_US";  // 生产时，需要删除
    QTranslator *translator = new QTranslator((QObject *)this);
    if (translator->load(QString(":/translations/rdm_") + locale)) {
        qDebug() << "Load translations file for locale:" << locale;
        QCoreApplication::installTranslator(translator);
    } else {
        delete translator;
    }
}

// 初始化python,添加import路径
void Application::initPython() {
    m_python = QSharedPointer<QPython>(new QPython(this, 1, 5));
    m_python->addImportPath("qrc:/python/");
#ifdef Q_OS_MACOS
    m_python->addImportPath(applicationDirPath() + "/../Resources/py");
#else
    m_python->addImportPath(applicationDirPath());
#endif
    qDebug() << "Python import path:" << applicationDirPath();
}



/* QString QCoreApplication::translate (const char * context, const char * sourceText, const char * disambiguation, Encoding encoding, int n)
这个才是真正进行翻译操作的函数，前面我们提到的tr最终是通过调用该函数来实现翻译功能的。

context 上下文，一般就是需要翻译的字符串所在的类的名字
sourceText 需要翻译的字符串。(我们关注的编码其实就是它的编码)
disambiguation 消除歧义用的。(比如我们的类内出现两处"close"，一处含义是关闭，另一处含义是亲密的。显然需要让翻译人员知道这点区别)
encoding 指定编码。它有两个值
    CodecForTr 使用setCodecForTr()设置的编码来解释 sourceText
    UnicodeUTF8 使用utf8编码来解释 sourceText
    其实这两个分别对应tr和trUtf8
n 处理单复数(对中文来说，不存在这个问题)
*/
// main.cpp中的方法
void Application::initModels() {
    ConfigManager confManager(m_settingsDir);    // 初始化configManager
    QString config = confManager.getApplicationConfigPath("connections.json");
    qDebug() << "===== Application::initModels === config =" << config << "========";
    if (config.isNull()) {
        QMessageBox::critical(
            nullptr,
            QCoreApplication::translate("RDM", "Settings directory is not writable"),
            QCoreApplication::translate("RDM", "RDM can't save connections file to settings directory. Please change file permissions or restart RDM as administrator."));

        throw std::runtime_error("invalid connections config");
    }

    /*QSharedPointer 是一个共享指针，它与 QScopedPointer 一样包装了new操作符在堆上分配的动态对象，但它实现的是引用计数型的智能指针 ，
     * 也就是说，与QScopedPointer不同的是，QSharedPointer可以被自由地拷贝和赋值，在任意的地方共享它，所以QSharedPointer也可以用作容器元素。
     *
     * 所谓的计数型指针，就是说在内部QSharedPointer对拥有的内存资源进行引用计数，比如有3个QSharedPointer同时指向一个内存资源，那么就计数3，直到引用计数下降到0，
     * 那么就自动去释放内存啦。
     *
     * 需要注意的是：QSharedPointer 是线程安全的，因此即使有多个线程同时修改 QSharedPointer 对象也不需要加锁。
     * 虽然 QSharedPointer 是线程安全的，但是 QSharedPointer 指向的内存区域可不一定是线程安全的。
     * 所以多个线程同时修改 QSharedPointer 指向的数据时还要应该考虑加锁。
    */

    m_keyFactory = QSharedPointer<KeyFactory>(new KeyFactory());
    m_keyValues = QSharedPointer<ValueEditor::TabsModel>(new ValueEditor::TabsModel(m_keyFactory.staticCast<ValueEditor::AbstractKeyFactory>(), m_events));
    // 设置信号槽 ,函数4个参数：发射信号的对象，发射的信号，接受信号的对象，要执行的槽；
    connect(m_events.data(), &Events::openValueTab, m_keyValues.data(), &ValueEditor::TabsModel::openTab);
    connect(m_events.data(), &Events::newKeyDialog, m_keyFactory.data(), &KeyFactory::createNewKeyRequest);
    connect(m_events.data(), &Events::closeDbKeys, m_keyValues.data(), &ValueEditor::TabsModel::closeDbKeys);
    // 配置连接信息
    m_connections = QSharedPointer<ConnectionsManager>(new ConnectionsManager(config, m_events));
    // appRendered事件后，调用ConnectionsManager的方法loadConnections()
    connect(m_events.data(), &Events::appRendered, this, [this]() {
        if (m_connections) {
            m_connections->loadConnections();
        }
    });


    // 设置批量操作 信号槽
    m_bulkOperations = QSharedPointer<BulkOperations::Manager>(new BulkOperations::Manager(m_connections, m_python));
    connect(m_events.data(), &Events::requestBulkOperation, m_bulkOperations.data(), &BulkOperations::Manager::requestBulkOperation);
    // 设置tabViewModel 信号槽
    m_consoleModel = QSharedPointer<TabViewModel>(new TabViewModel(getTabModelFactory<Console::Model>()));
    connect(m_events.data(), &Events::openConsole, m_consoleModel.data(), &TabViewModel::openTab);

    // 设置redisClient，及信号槽
    auto srvStatsFactory = [this](QSharedPointer<RedisClient::Connection> c, int dbIndex, QList<QByteArray> initCmd) {
        // rawModelPtr的Model()构造函数的三个参数是connection, dbIndex, initCmd
        auto rawModelPtr = new ServerStats::Model(c, dbIndex, initCmd);
        auto model = QSharedPointer<TabModel>(rawModelPtr, &QObject::deleteLater);
        QObject::connect(rawModelPtr, &ServerStats::Model::openConsoleTerminal, m_events.data(), &Events::openConsole);
        return model;
    };
    // 设置openTab 信号槽, 调用TabModel的openTab(c)函数
    m_serverStatsModel = QSharedPointer<TabViewModel>(new TabViewModel(srvStatsFactory));
    connect(m_events.data(), &Events::openServerStats, this, [this](QSharedPointer<RedisClient::Connection> c) {
        m_serverStatsModel->openTab(c);
    });

// 设置外部formatter, 以及信号槽
#ifdef ENABLE_EXTERNAL_FORMATTERS
    m_formattersManager = QSharedPointer<ValueEditor::ExternalFormattersManager>(new ValueEditor::ExternalFormattersManager());
    connect(m_formattersManager.data(), &ValueEditor::ExternalFormattersManager::error, this, [this](const QString & msg) {
        qDebug() << "External formatters:" << msg;
        m_events->log(QString("External: %1").arg(msg));
    });
    if (!m_formattersDir.isEmpty()) {
        m_formattersManager->setPath(m_formattersDir);
    }
    // QtConcurrent 是命名空间 (namespace)，它提供了高层次的函数接口 (APIs)，使所写程序，可根据计算机的 CPU 核数，自动调整运行的线程数目。
    connect(m_events.data(), &Events::appRendered, this, [this]() {
        QtConcurrent::run([this]() {
            if (m_formattersManager) {
                m_formattersManager->loadFormatters();
            }
            if (m_events) {
                emit m_events->externalFormattersLoaded();
            }
        });
    });
#endif
// 设置内嵌formatter, 以及信号槽
    m_embeddedFormatters = QSharedPointer<ValueEditor::EmbeddedFormattersManager>(new ValueEditor::EmbeddedFormattersManager(m_python));
    connect(m_embeddedFormatters.data(), &ValueEditor::EmbeddedFormattersManager::error, this, [this](const QString & msg) {
        qDebug() << "Internal formatters:" << msg;
        m_events->log(QString("Formatters: %1").arg(msg));
    });
    m_consoleAutocompleteModel = QSharedPointer<Console::AutocompleteModel>(new Console::AutocompleteModel());
}


// 初始化 qml部分
void Application::initQml() {
    qDebug() << "========= Application::initQml() Start ==========";
    if (m_renderingBackend == "auto") {
        QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Software);
    } else {
        QQuickWindow::setSceneGraphBackend(m_renderingBackend);
    }
    registerQmlTypes();
    registerQmlRootObjects();
    // 载入app.qml文件
    qDebug("---------------------1--------------------------");
    try {
        m_engine.load(QUrl(QStringLiteral("qrc:///app.qml")));
    } catch (...) {
        qDebug() << "Failed to load app window. Retrying with software renderer...";
        QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Software);
        m_engine.load(QUrl(QStringLiteral("qrc:///app.qml")));
    }
    qDebug("---------------------2--------------------------");
    updatePalette();
    // 设置信号槽
    connect(this, &QGuiApplication::paletteChanged, this, &Application::updatePalette);
    qDebug() << "Rendering backend:" << QQuickWindow::sceneGraphBackend();
    emit m_events->appRendered(); // 发送消息出去
    qDebug() << "========= Application::initQml() Finish ==========";
}

// 为qml注册type, 组，大版本号，小版本号，名称
void Application::registerQmlTypes() {
    qmlRegisterType<SortFilterProxyModel>("rdm.models", 1, 0, "SortFilterProxyModel");
    qmlRegisterType<SyntaxHighlighter>("rdm.models", 1, 0, "SyntaxHighlighter");
    qmlRegisterType<TextCharFormat>("rdm.models", 1, 0, "TextCharFormat");
    qRegisterMetaType<ServerConfig>();
}

// 为QQmlApplicationEngine注册object，这样在qml文件中，可以调用c++中的类
void Application::registerQmlRootObjects() {
    m_engine.rootContext()->setContextProperty("appEvents", m_events.data());
    m_engine.rootContext()->setContextProperty("qmlUtils", m_qmlUtils.data());
    m_engine.rootContext()->setContextProperty("connectionsManager", m_connections.data());
    m_engine.rootContext()->setContextProperty("keyFactory", m_keyFactory.data());
    m_engine.rootContext()->setContextProperty("valuesModel", m_keyValues.data());
#ifdef ENABLE_EXTERNAL_FORMATTERS
    m_engine.rootContext()->setContextProperty("formattersManager", m_formattersManager.data());
#endif
    m_engine.rootContext()->setContextProperty("embeddedFormattersManager", m_embeddedFormatters.data());
    m_engine.rootContext()->setContextProperty("consoleModel", m_consoleModel.data());
    m_engine.rootContext()->setContextProperty("serverStatsModel", m_serverStatsModel.data());
    m_engine.rootContext()->setContextProperty("bulkOperations", m_bulkOperations.data());
    m_engine.rootContext()->setContextProperty("consoleAutocompleteModel", m_consoleAutocompleteModel.data());
}

// 为QQmlApplicationEngine更新调色板
void Application::updatePalette() {
    if (m_engine.rootObjects().size() == 0) {
        qWarning() << "Cannot update palette. Root object is not loaded.";
        return;
    }
    m_engine.rootObjects().at(0)->setProperty("palette", QGuiApplication::palette());
}



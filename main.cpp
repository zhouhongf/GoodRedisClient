#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>

#if defined(Q_OS_WIN) | defined(Q_OS_LINUX)
    #include <QProcess>
    #define RELAUNCH_CODE 1001
#endif

#ifdef CRASHPAD_INTEGRATION
    #include "crashpad/handler.h"
#endif

#ifdef LINUX_SIGNALS
    #include <sigwatch.h>
#endif

#include "app/app.h"




// 程序主入口
int main(int argc, char *argv[]) {
    int returnCode = 0;
// 如果定义了 崩溃处理，则 定义文件名称 和 绝对路径
#ifdef CRASHPAD_INTEGRATION
    QFileInfo appPath(QString::fromLocal8Bit(argv[0]));
    QString appDir(appPath.absoluteDir().path());
    startCrashpad(appDir);
#endif



#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    bool disableAutoScaling = false;

// 如果没有定义 禁止窗口缩放，则定义 主屏、宽度小于等于1920、像素比为1
#ifndef DISABLE_SCALING_TEST
    {
        QGuiApplication tmp(argc, argv);
        disableAutoScaling = QGuiApplication::primaryScreen()
                             && QGuiApplication::primaryScreen()->availableSize().width() <= 1920
                             && QGuiApplication::primaryScreen()->devicePixelRatio() == 1;
    }
#endif

// 如果时 Linux系统 且 定义了 禁止窗口缩放，则 disableAutoScaling = true
#if defined (Q_OS_LINUX) && defined(DISABLE_SCALING_TEST)
    disableAutoScaling = true;
#endif

// 根据 disableAutoScaling 的值，QGuiApplication设置属性
    if (disableAutoScaling) {
        qDebug() << "Disable auto-scaling";
        QGuiApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    } else {
        qDebug() << "Enable auto-scaling";
        QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    }

#endif


    // Application 结构函数初始化
    Application a(argc, argv);

// 如果定义了 LINUX信号，则设置信号槽 connect()函数
#ifdef LINUX_SIGNALS
    UnixSignalWatcher sigwatch;
    sigwatch.watchForSignal(SIGINT);
    sigwatch.watchForSignal(SIGTERM);
    QObject::connect(&sigwatch, SIGNAL(unixSignal(int)), &a, SLOT(quit()));
#endif

    a.initModels();                 // 初始化 模型
    a.initQml();                    // 初始化 qml
    returnCode = a.exec();

// 定义 returnCode
#if defined(Q_OS_WIN) | defined(Q_OS_LINUX)
    if (returnCode == RELAUNCH_CODE) {
        QProcess::startDetached(a.arguments()[0], a.arguments());
        returnCode = 0;
    }
#endif
    return returnCode;
}


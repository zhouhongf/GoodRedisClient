#include "events.h"


void Events::registerLoggerForConnection(RedisClient::Connection& c) {
  auto self = sharedFromThis().toWeakRef();

  // 设置信号槽 ,函数4个参数：发射信号的对象，发射的信号，接受信号的对象，要执行的槽；
  QObject::connect(&c, &RedisClient::Connection::log, this, [self](const QString& info) {
        if (!self) return;
        emit self.toStrongRef()->log(QString("Connection: %1").arg(info));
      }, Qt::QueuedConnection);

  QObject::connect(&c, &RedisClient::Connection::error, this, [self](const QString& error) {
        if (!self) return;
        emit self.toStrongRef()->log(QString("Connection: %1").arg(error));
      }, Qt::QueuedConnection);
}

#include "keymodelhash.h"
#include "connection.h"
#include <QObject>



HashKeyModel::HashKeyModel(QSharedPointer<RedisClient::Connection> connection, QByteArray fullPath, int dbIndex, long long ttl) : KeyModel(connection, fullPath, dbIndex, ttl, "HLEN", "HSCAN") {}

QString HashKeyModel::type() { return "hash"; }

QHash<int, QByteArray> HashKeyModel::getRoles() {
  QHash<int, QByteArray> roles;
  roles[Roles::RowNumber] = "rowNumber";
  roles[Roles::Key] = "key";
  roles[Roles::Value] = "value";
  return roles;
}


QStringList HashKeyModel::getColumnNames() {
  return QStringList() << "rowNumber" << "key" << "value";
}

QVariant HashKeyModel::getData(int rowIndex, int dataRole) {
  // 如果不存在row，则返回空值
  if (!isRowLoaded(rowIndex)) { return QVariant(); }

  // 根据 dataRole，从row缓存中取出相关的值
  QPair<QByteArray, QByteArray> row = m_rowsCache[rowIndex];
  if (dataRole == Roles::Key){
    return row.first;
  }else if (dataRole == Roles::Value){
    return row.second;
  }else if (dataRole == Roles::RowNumber){
    return rowIndex;
  }
  return QVariant();
}



void HashKeyModel::updateRow(int rowIndex, const QVariantMap &row, Callback c) {
  if (!isRowLoaded(rowIndex) || !isRowValid(row)) {
    c(QCoreApplication::translate("RDM", "Invalid row"));
    return;
  }

  QPair<QByteArray, QByteArray> cachedRow = m_rowsCache[rowIndex];

  bool keyChanged = cachedRow.first != row["key"].toString();
  bool valueChanged = cachedRow.second != row["value"].toString();

  QByteArray rowkey = (keyChanged) ? row["key"].toByteArray() : cachedRow.first;
  QByteArray rowvalue = (valueChanged) ? row["value"].toByteArray() : cachedRow.second;
  QPair<QByteArray, QByteArray> newRow(rowkey, rowvalue);

  // 执行 值更新后的动作，如果err是空值，则 MappedCache<T> m_rowsCache 使用 newRow 替换
  auto afterValueUpdate = [this, c, rowIndex, newRow](const QString &err) {
    if (err.isEmpty()) { m_rowsCache.replace(rowIndex, newRow); }
    return c(err);
  };

  // 如果 key改变了，则执行deleteHashRow()方法后，重新执行setHashRow()方法
  // 如果 key没有变，则直接执行setHashRow()方法
  if (keyChanged) {
    deleteHashRow(cachedRow.first,
                  [this, c, newRow, afterValueUpdate](const QString &err) {
                    if (err.size() > 0) { return c(err);}
                    setHashRow(newRow.first, newRow.second, afterValueUpdate);
                  });
  } else {
    setHashRow(newRow.first, newRow.second, afterValueUpdate);
  }
}

void HashKeyModel::addRow(const QVariantMap &row, Callback c) {
  if (!isRowValid(row)) {
    c(QCoreApplication::translate("RDM", "Invalid row"));
    return;
  }

  setHashRow(row["key"].toByteArray(),
             row["value"].toByteArray(),
             [this, c](const QString &err) {
                if (err.isEmpty()) { m_rowCount++; }
                return c(err);
             },
             false);
}

void HashKeyModel::removeRow(int i, Callback c) {
  if (!isRowLoaded(i)) { return; }

  QPair<QByteArray, QByteArray> row = m_rowsCache[i];

  deleteHashRow(row.first,
                [this, i, c](const QString &err) {
                    if (err.isEmpty()) {
                      m_rowCount--;
                      m_rowsCache.removeAt(i);
                      setRemovedIfEmpty();
                    }

                    return c(err);
                });
}





void HashKeyModel::setHashRow(const QByteArray &hashKey, const QByteArray &hashValue, Callback c, bool updateIfNotExist) {
  // 根据 updateIfNotExist 的 bool值,来定义rawCmd
  QList<QByteArray> rawCmd{(updateIfNotExist) ? "HSET" : "HSETNX", m_keyFullPath, hashKey, hashValue};
  // 执行命令，如果 updateIfNotExist == false 且 response 返回值为0， 则返回提示 相同key的value已存在
  executeCmd(rawCmd,
             c,
             [updateIfNotExist](RedisClient::Response r, Callback c) {
               if (updateIfNotExist == false && r.value().toInt() == 0) {
                 return c(QCoreApplication::translate("RDM", "Value with the same key already exists"));
               } else {
                 return c(QString());
               }
             });
}

void HashKeyModel::deleteHashRow(const QByteArray &hashKey, Callback c) {
  executeCmd({"HDEL", m_keyFullPath, hashKey}, c);
}




// 将载入的row添加到cache中
int HashKeyModel::addLoadedRowsToCache(const QVariantList &rows, QVariant rowStartId) {
  QList<QPair<QByteArray, QByteArray>> result;

  // 使用STL只读遍历
  for (QVariantList::const_iterator item = rows.begin(); item != rows.end(); ++item) {
    QPair<QByteArray, QByteArray> value;
    value.first = item->toByteArray();
    ++item;

    // 如果到了行最后，则发出 错误提示
    if (item == rows.end()) {
      emit m_notifier->error(QCoreApplication::translate("RDM", "Data was loaded from server partially."));
      return 0;
    }

    value.second = item->toByteArray();
    result.push_back(value);
  }

  auto rowStart = rowStartId.toLongLong();
  m_rowsCache.addLoadedRange({rowStart, rowStart + result.size() - 1}, result);

  return result.size();
}

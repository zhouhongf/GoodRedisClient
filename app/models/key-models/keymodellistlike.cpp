#include "keymodellistlike.h"

ListLikeKeyModel::ListLikeKeyModel(QSharedPointer<RedisClient::Connection> connection, QByteArray fullPath, int dbIndex, long long ttl, QByteArray rowsCountCmd, QByteArray rowsLoadCmd) : KeyModel(connection, fullPath, dbIndex, ttl, rowsCountCmd, rowsLoadCmd) {}



QHash<int, QByteArray> ListLikeKeyModel::getRoles() {
  QHash<int, QByteArray> roles;
  roles[Roles::Value] = "value";
  roles[Roles::RowNumber] = "rowNumber";
  return roles;
}

QStringList ListLikeKeyModel::getColumnNames() {
  return QStringList() << "rowNumber" << "value";
}

QVariant ListLikeKeyModel::getData(int rowIndex, int dataRole) {
  if (!isRowLoaded(rowIndex)) { return QVariant(); }
  // 根据 dataRole 来从 缓存row中取值，或直接返回rowIndex
  switch (dataRole) {
    case Value:
      return m_rowsCache[rowIndex];
    case RowNumber:
      return rowIndex;
  }
  return QVariant();
}


int ListLikeKeyModel::addLoadedRowsToCache(const QVariantList &rows, QVariant rowStartId) {
  QList<QByteArray> result;
  auto rowStart = rowStartId.toLongLong();

  foreach (QVariant row, rows) {
      result.push_back(row.toByteArray());
  }

  m_rowsCache.addLoadedRange({rowStart, rowStart + result.size() - 1}, result);
  return result.size();
}

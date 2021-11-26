#pragma once
#include "abstractkeymodel.h"



class HashKeyModel : public KeyModel<QPair<QByteArray, QByteArray>> {
 public:
  HashKeyModel(QSharedPointer<RedisClient::Connection> connection, QByteArray fullPath, int dbIndex, long long ttl);

  QString type() override;
  QHash<int, QByteArray> getRoles() override;
  QStringList getColumnNames() override;

  QVariant getData(int rowIndex, int dataRole) override;

  virtual void updateRow(int rowIndex, const QVariantMap &, Callback) override;
  void addRow(const QVariantMap &, Callback) override;
  void removeRow(int, Callback) override;

 protected:
  int addLoadedRowsToCache(const QVariantList &list, QVariant rowStart) override;

 private:
  enum Roles { RowNumber = Qt::UserRole + 1, Key, Value };

  void setHashRow(const QByteArray &hashKey, const QByteArray &hashValue, Callback c, bool updateIfNotExist = true);
  void deleteHashRow(const QByteArray &hashKey, Callback c);
};

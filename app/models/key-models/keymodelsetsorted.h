#pragma once
#include "abstractkeymodel.h"

class SortedSetKeyModel : public KeyModel<QPair<QByteArray, QByteArray>> {
public:
    SortedSetKeyModel(QSharedPointer<RedisClient::Connection> connection, QByteArray fullPath, int dbIndex, long long ttl);

    QString type() override;

    QHash<int, QByteArray> getRoles() override;
    QStringList getColumnNames() override;
    QVariant getData(int rowIndex, int dataRole) override;

    virtual void updateRow(int rowIndex, const QVariantMap&, Callback c) override;
    void addRow(const QVariantMap&, Callback c) override;
    void removeRow(int, Callback c) override;

protected:
    int addLoadedRowsToCache(const QVariantList& list, QVariant rowStart) override;

private:
    enum Roles { RowNumber = Qt::UserRole + 1, Value, Score };

    void addSortedSetRow(const QByteArray& value, QByteArray score, Callback c, bool updateExisting = false);
    void deleteSortedSetRow(const QByteArray& value, Callback c);
};

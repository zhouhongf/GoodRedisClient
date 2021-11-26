#pragma once
#include "abstractkeymodel.h"



class StringKeyModel : public KeyModel<QByteArray> {
public:
    StringKeyModel(QSharedPointer<RedisClient::Connection> connection, QByteArray fullPath, int dbIndex, long long ttl);

    QString type() override;

    QHash<int, QByteArray> getRoles() override;
    QStringList getColumnNames() override;
    QVariant getData(int rowIndex, int dataRole) override;

    virtual void updateRow(int rowIndex, const QVariantMap& row, Callback c) override;
    void addRow(const QVariantMap&, Callback c) override;
    void loadRows(QVariant, unsigned long, LoadRowsCallback callback) override;
    void removeRow(int, Callback c) override;

    virtual unsigned long rowsCount() override { return m_rowCount; }

protected:
    int addLoadedRowsToCache(const QVariantList&, QVariant) override { return 1; }

private:
    enum Roles { Value = Qt::UserRole + 1 };

    QString m_type;
};

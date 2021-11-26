#pragma once
#include "abstractkeymodel.h"


class ListLikeKeyModel : public KeyModel<QByteArray> {
public:
    ListLikeKeyModel(QSharedPointer<RedisClient::Connection> connection, QByteArray fullPath, int dbIndex, long long ttl, QByteArray rowsCountCmd, QByteArray rowsLoadCmd);

    QHash<int, QByteArray> getRoles() override;
    QStringList getColumnNames() override;
    QVariant getData(int rowIndex, int dataRole) override;

protected:
    enum Roles { RowNumber = Qt::UserRole + 1, Value };

protected:
    int addLoadedRowsToCache(const QVariantList& rows, QVariant rowStart) override;
};

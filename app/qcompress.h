#pragma once
#include <QByteArray>
#include <QString>

namespace qcompress {

    enum { UNKNOWN, GZIP, LZ4 };

    unsigned guessFormat(const QByteArray& val);

    QString nameOf(unsigned alg);

    QByteArray compress(const QByteArray& val, unsigned algo);
    QByteArray decompress(const QByteArray& val);

}  // namespace qcompress

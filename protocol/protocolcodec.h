#ifndef PROTOCOLCODEC_H
#define PROTOCOLCODEC_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

class ProtocolCodec
{
public:
    enum class DecodeStatus
    {
        MessageReady,
        NeedMoreData,
        InvalidFrame
    };

    struct DecodeResult
    {
        DecodeStatus status = DecodeStatus::NeedMoreData;
        QJsonObject message;
        QString error;
    };

    static constexpr qsizetype maxFrameBytes = 1024 * 1024;
    static constexpr int currentVersion = 1;

    static QJsonObject createEnvelope(
        const QString &type,
        const QString &requestId,
        const QJsonObject &payload = QJsonObject{});

    static QByteArray encode(
        const QJsonObject &messageObject);

    static DecodeResult tryDecode(
        QByteArray &buffer);
};

#endif // PROTOCOLCODEC_H

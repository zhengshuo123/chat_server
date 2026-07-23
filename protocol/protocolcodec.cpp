#include "protocolcodec.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>

namespace
{
constexpr qsizetype headerBytes = 4;

quint32 readBigEndianLength(const QByteArray &buffer)
{
    return (static_cast<quint32>(
                static_cast<unsigned char>(buffer.at(0)))
            << 24)
        | (static_cast<quint32>(
               static_cast<unsigned char>(buffer.at(1)))
           << 16)
        | (static_cast<quint32>(
               static_cast<unsigned char>(buffer.at(2)))
           << 8)
        | static_cast<quint32>(
              static_cast<unsigned char>(buffer.at(3)));
}

void appendBigEndianLength(
    QByteArray &buffer,
    quint32 length)
{
    buffer.append(
        static_cast<char>((length >> 24) & 0xff));
    buffer.append(
        static_cast<char>((length >> 16) & 0xff));
    buffer.append(
        static_cast<char>((length >> 8) & 0xff));
    buffer.append(
        static_cast<char>(length & 0xff));
}
}

QJsonObject ProtocolCodec::createEnvelope(
    const QString &type,
    const QString &requestId,
    const QJsonObject &payload)
{
    QJsonObject envelope;

    envelope.insert(
        QStringLiteral("version"),
        currentVersion);
    envelope.insert(
        QStringLiteral("type"),
        type);
    envelope.insert(
        QStringLiteral("request_id"),
        requestId);
    envelope.insert(
        QStringLiteral("timestamp"),
        QString::number(
            QDateTime::currentMSecsSinceEpoch()));
    envelope.insert(
        QStringLiteral("payload"),
        payload);

    return envelope;
}

QByteArray ProtocolCodec::encode(
    const QJsonObject &messageObject)
{
    const QJsonDocument document(
        messageObject);
    const QByteArray payload =
        document.toJson(QJsonDocument::Compact);

    if (payload.size() > maxFrameBytes)
    {
        return QByteArray{};
    }

    QByteArray frame;
    frame.reserve(
        static_cast<qsizetype>(headerBytes)
        + payload.size());

    appendBigEndianLength(
        frame,
        static_cast<quint32>(payload.size()));
    frame.append(payload);

    return frame;
}

ProtocolCodec::DecodeResult ProtocolCodec::tryDecode(
    QByteArray &buffer)
{
    if (buffer.size() < headerBytes)
    {
        return DecodeResult{};
    }

    const quint32 payloadLength =
        readBigEndianLength(buffer);

    if (payloadLength == 0)
    {
        buffer.clear();
        return DecodeResult{
            DecodeStatus::InvalidFrame,
            QJsonObject{},
            QStringLiteral("协议帧长度不能为 0")};
    }

    if (payloadLength > maxFrameBytes)
    {
        buffer.clear();
        return DecodeResult{
            DecodeStatus::InvalidFrame,
            QJsonObject{},
            QStringLiteral("协议帧超过最大长度")};
    }

    const qsizetype frameBytes =
        headerBytes
        + static_cast<qsizetype>(payloadLength);

    if (buffer.size() < frameBytes)
    {
        return DecodeResult{};
    }

    const QByteArray payload =
        buffer.mid(
            headerBytes,
            static_cast<qsizetype>(payloadLength));

    buffer.remove(0, frameBytes);

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(
            payload,
            &parseError);

    if (parseError.error != QJsonParseError::NoError
        || !document.isObject())
    {
        return DecodeResult{
            DecodeStatus::InvalidFrame,
            QJsonObject{},
            QStringLiteral("协议帧包含无效 JSON")};
    }

    return DecodeResult{
        DecodeStatus::MessageReady,
        document.object(),
        QString{}};
}

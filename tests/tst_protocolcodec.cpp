#include "../protocol/protocolcodec.h"

#include <QJsonObject>
#include <QtTest/QtTest>

class ProtocolCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void encodesAndDecodesEnvelope();
    void waitsForPartialFrame();
    void decodesConcatenatedFrames();
    void rejectsInvalidJsonFrame();
    void rejectsOversizedFrame();
};

void ProtocolCodecTest::encodesAndDecodesEnvelope()
{
    QJsonObject payload;
    payload.insert(
        QStringLiteral("content"),
        QStringLiteral("hello"));

    const QJsonObject envelope =
        ProtocolCodec::createEnvelope(
            QStringLiteral("message.send"),
            QStringLiteral("req-1"),
            payload);

    QByteArray buffer =
        ProtocolCodec::encode(envelope);

    QVERIFY(buffer.size() > 4);

    const ProtocolCodec::DecodeResult result =
        ProtocolCodec::tryDecode(buffer);

    QCOMPARE(
        result.status,
        ProtocolCodec::DecodeStatus::MessageReady);
    QCOMPARE(
        result.message.value(QStringLiteral("version")).toInt(),
        ProtocolCodec::currentVersion);
    QCOMPARE(
        result.message.value(QStringLiteral("type")).toString(),
        QStringLiteral("message.send"));
    QCOMPARE(
        result.message.value(QStringLiteral("request_id")).toString(),
        QStringLiteral("req-1"));
    QCOMPARE(
        result.message.value(QStringLiteral("payload")).toObject()
            .value(QStringLiteral("content")).toString(),
        QStringLiteral("hello"));
    QVERIFY(buffer.isEmpty());
}

void ProtocolCodecTest::waitsForPartialFrame()
{
    const QJsonObject envelope =
        ProtocolCodec::createEnvelope(
            QStringLiteral("ping"),
            QStringLiteral("req-2"));

    const QByteArray frame =
        ProtocolCodec::encode(envelope);
    QByteArray buffer =
        frame.left(frame.size() / 2);

    const ProtocolCodec::DecodeResult partialResult =
        ProtocolCodec::tryDecode(buffer);

    QCOMPARE(
        partialResult.status,
        ProtocolCodec::DecodeStatus::NeedMoreData);

    buffer.append(
        frame.mid(frame.size() / 2));

    const ProtocolCodec::DecodeResult completeResult =
        ProtocolCodec::tryDecode(buffer);

    QCOMPARE(
        completeResult.status,
        ProtocolCodec::DecodeStatus::MessageReady);
    QCOMPARE(
        completeResult.message.value(QStringLiteral("type")).toString(),
        QStringLiteral("ping"));
}

void ProtocolCodecTest::decodesConcatenatedFrames()
{
    const QByteArray firstFrame =
        ProtocolCodec::encode(
            ProtocolCodec::createEnvelope(
                QStringLiteral("first"),
                QStringLiteral("req-3")));
    const QByteArray secondFrame =
        ProtocolCodec::encode(
            ProtocolCodec::createEnvelope(
                QStringLiteral("second"),
                QStringLiteral("req-4")));

    QByteArray buffer =
        firstFrame + secondFrame;

    const ProtocolCodec::DecodeResult firstResult =
        ProtocolCodec::tryDecode(buffer);
    const ProtocolCodec::DecodeResult secondResult =
        ProtocolCodec::tryDecode(buffer);

    QCOMPARE(
        firstResult.status,
        ProtocolCodec::DecodeStatus::MessageReady);
    QCOMPARE(
        secondResult.status,
        ProtocolCodec::DecodeStatus::MessageReady);
    QCOMPARE(
        firstResult.message.value(QStringLiteral("type")).toString(),
        QStringLiteral("first"));
    QCOMPARE(
        secondResult.message.value(QStringLiteral("type")).toString(),
        QStringLiteral("second"));
    QVERIFY(buffer.isEmpty());
}

void ProtocolCodecTest::rejectsInvalidJsonFrame()
{
    QByteArray buffer;
    buffer.append(char(0));
    buffer.append(char(0));
    buffer.append(char(0));
    buffer.append(char(4));
    buffer.append("oops");

    const ProtocolCodec::DecodeResult result =
        ProtocolCodec::tryDecode(buffer);

    QCOMPARE(
        result.status,
        ProtocolCodec::DecodeStatus::InvalidFrame);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(buffer.isEmpty());
}

void ProtocolCodecTest::rejectsOversizedFrame()
{
    const quint32 oversizedLength =
        static_cast<quint32>(
            ProtocolCodec::maxFrameBytes + 1);

    QByteArray buffer;
    buffer.append(
        static_cast<char>((oversizedLength >> 24) & 0xff));
    buffer.append(
        static_cast<char>((oversizedLength >> 16) & 0xff));
    buffer.append(
        static_cast<char>((oversizedLength >> 8) & 0xff));
    buffer.append(
        static_cast<char>(oversizedLength & 0xff));

    const ProtocolCodec::DecodeResult result =
        ProtocolCodec::tryDecode(buffer);

    QCOMPARE(
        result.status,
        ProtocolCodec::DecodeStatus::InvalidFrame);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(buffer.isEmpty());
}

QTEST_MAIN(ProtocolCodecTest)

#include "tst_protocolcodec.moc"

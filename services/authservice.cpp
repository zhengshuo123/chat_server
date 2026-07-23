#include "authservice.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QStringList>

namespace
{
constexpr qsizetype saltBytes = 32;
constexpr qsizetype hashBytes = 32;
constexpr int minimumPasswordLength = 8;

QString normalizedUsername(
    const QString &username)
{
    return username.trimmed();
}
}

AuthService::AuthService(
    SQLiteRepository &repository,
    int iterations)
    : m_repository(repository)
    , m_iterations(qMax(1000, iterations))
{
}

AuthService::Result AuthService::registerUser(
    const QString &username,
    const QString &password)
{
    const QString normalized =
        normalizedUsername(username);

    if (normalized.isEmpty())
    {
        return Result{false, QStringLiteral("用户名不能为空"), 0};
    }

    if (password.size() < minimumPasswordLength)
    {
        return Result{false, QStringLiteral("密码至少需要 8 个字符"), 0};
    }

    if (m_repository.userId(normalized) > 0)
    {
        return Result{false, QStringLiteral("用户名已存在"), 0};
    }

    const QByteArray salt =
        secureRandomBytes(saltBytes);
    const QString saltText =
        QString::fromLatin1(salt.toBase64());
    const QString passwordHash =
        hashPassword(password, salt, m_iterations);

    if (!m_repository.createUser(
            normalized,
            passwordHash,
            saltText))
    {
        return Result{false, m_repository.lastError(), 0};
    }

    return Result{
        true,
        QStringLiteral("注册成功"),
        m_repository.userId(normalized)};
}

AuthService::Result AuthService::verifyLogin(
    const QString &username,
    const QString &password) const
{
    const QString normalized =
        normalizedUsername(username);
    const SQLiteRepository::UserCredentials credentials =
        m_repository.credentialsForUser(normalized);

    if (credentials.id == 0)
    {
        return Result{false, QStringLiteral("用户名或密码错误"), 0};
    }

    int iterations = 0;
    QByteArray expectedHash;

    if (!parseHash(
            credentials.passwordHash,
            &iterations,
            &expectedHash))
    {
        return Result{false, QStringLiteral("账户凭证格式无效"), 0};
    }

    const QByteArray salt =
        QByteArray::fromBase64(
            credentials.passwordSalt.toLatin1());
    const QString candidateHashText =
        hashPassword(password, salt, iterations);

    int ignoredIterations = 0;
    QByteArray candidateHash;

    if (!parseHash(
            candidateHashText,
            &ignoredIterations,
            &candidateHash))
    {
        return Result{false, QStringLiteral("账户凭证格式无效"), 0};
    }

    if (!constantTimeEquals(expectedHash, candidateHash))
    {
        return Result{false, QStringLiteral("用户名或密码错误"), 0};
    }

    return Result{true, QStringLiteral("登录成功"), credentials.id};
}

QString AuthService::hashPassword(
    const QString &password,
    const QByteArray &salt,
    int iterations)
{
    const QByteArray hash =
        pbkdf2HmacSha256(
            password.toUtf8(),
            salt,
            qMax(1000, iterations),
            hashBytes);

    return QStringLiteral("pbkdf2_sha256$%1$%2")
        .arg(qMax(1000, iterations))
        .arg(QString::fromLatin1(hash.toBase64()));
}

QByteArray AuthService::pbkdf2HmacSha256(
    const QByteArray &password,
    const QByteArray &salt,
    int iterations,
    qsizetype outputBytes)
{
    QByteArray derived;
    derived.reserve(outputBytes);

    quint32 blockIndex = 1;

    while (derived.size() < outputBytes)
    {
        QByteArray blockSalt = salt;
        blockSalt.append(static_cast<char>((blockIndex >> 24) & 0xff));
        blockSalt.append(static_cast<char>((blockIndex >> 16) & 0xff));
        blockSalt.append(static_cast<char>((blockIndex >> 8) & 0xff));
        blockSalt.append(static_cast<char>(blockIndex & 0xff));

        QByteArray u =
            QMessageAuthenticationCode::hash(
                blockSalt,
                password,
                QCryptographicHash::Sha256);
        QByteArray t = u;

        for (int i = 1; i < iterations; ++i)
        {
            u = QMessageAuthenticationCode::hash(
                u,
                password,
                QCryptographicHash::Sha256);

            for (qsizetype j = 0; j < t.size(); ++j)
            {
                t[j] = static_cast<char>(t.at(j) ^ u.at(j));
            }
        }

        derived.append(t);
        ++blockIndex;
    }

    derived.truncate(outputBytes);
    return derived;
}

QByteArray AuthService::secureRandomBytes(
    qsizetype size)
{
    QByteArray bytes;
    bytes.resize(size);

    QRandomGenerator *generator =
        QRandomGenerator::system();

    for (qsizetype i = 0; i < size; i += 8)
    {
        const quint64 value =
            generator->generate64();
        const qsizetype chunk =
            qMin<qsizetype>(8, size - i);

        for (qsizetype j = 0; j < chunk; ++j)
        {
            bytes[i + j] =
                static_cast<char>(
                    (value >> (8 * j)) & 0xff);
        }
    }

    return bytes;
}

bool AuthService::constantTimeEquals(
    const QByteArray &left,
    const QByteArray &right)
{
    if (left.size() != right.size())
    {
        return false;
    }

    unsigned char difference = 0;

    for (qsizetype i = 0; i < left.size(); ++i)
    {
        difference |=
            static_cast<unsigned char>(left.at(i))
            ^ static_cast<unsigned char>(right.at(i));
    }

    return difference == 0;
}

bool AuthService::parseHash(
    const QString &encodedHash,
    int *iterations,
    QByteArray *hashOutput)
{
    const QStringList parts =
        encodedHash.split(QLatin1Char('$'));

    if (parts.size() != 3
        || parts.at(0) != QStringLiteral("pbkdf2_sha256"))
    {
        return false;
    }

    bool ok = false;
    const int parsedIterations =
        parts.at(1).toInt(&ok);

    if (!ok || parsedIterations < 1000)
    {
        return false;
    }

    if (iterations != nullptr)
    {
        *iterations = parsedIterations;
    }

    if (hashOutput != nullptr)
    {
        *hashOutput =
            QByteArray::fromBase64(
                parts.at(2).toLatin1());
    }

    return hashOutput == nullptr
        || hashOutput->size() == hashBytes;
}

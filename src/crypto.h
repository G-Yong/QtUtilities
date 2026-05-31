/*
 * crypto.h — Hashing & encoding utilities (single-file header library, C++17 inline style)
 *
 * Requires C++17 or later (for inline variables/functions).
 * Requires only Qt Core — no extra Qt modules.
 *   QT     += core
 *   CONFIG += c++17
 *
 * USAGE
 * -----
 * Just #include this header anywhere you need it — no .cpp compilation required.
 * Every function lives in the QtUtil namespace. These helpers wrap Qt's
 * QCryptographicHash / QMessageAuthenticationCode / QByteArray facilities in a
 * compact, string-friendly API. They are for integrity/encoding purposes
 * (checksums, ETags, cache keys, basic obfuscation), not a substitute for a
 * vetted cryptography library when strong confidentiality is required.
 *
 * QUICK START
 * -----------
 *   #include "crypto.h"
 *
 *   QString a = QtUtil::md5("hello");                 // 32-char hex
 *   QString b = QtUtil::sha256("hello");              // 64-char hex
 *   QString c = QtUtil::hmacSha256("msg", "key");     // keyed hash, hex
 *
 *   QString  b64 = QtUtil::base64Encode("你好");      // text -> base64
 *   QString  txt = QtUtil::base64DecodeText(b64);     // base64 -> text
 *
 *   QString  url = QtUtil::base64UrlEncode(data);     // URL-safe, no padding
 *   QString  hex = QtUtil::toHex(QByteArray("\x1f", 1));
 *
 *   QByteArray scrambled = QtUtil::xorCipher(data, "secret"); // symmetric
 *
 * FUNCTIONS
 * ---------
 *   QString    hashHex          (const QByteArray &data, QCryptographicHash::Algorithm algo)
 *   QString    md5              (const QByteArray/ QString &data)
 *   QString    sha1             (const QByteArray/ QString &data)
 *   QString    sha256           (const QByteArray/ QString &data)
 *   QString    sha512           (const QByteArray/ QString &data)
 *   QString    hmacHex          (const QByteArray &data, const QByteArray &key,
 *                                QCryptographicHash::Algorithm algo)
 *   QString    hmacSha256       (const QByteArray/ QString &data, const QByteArray/ QString &key)
 *   QString    base64Encode     (const QByteArray/ QString &data)
 *   QByteArray base64Decode     (const QString &b64)
 *   QString    base64DecodeText (const QString &b64)
 *   QString    base64UrlEncode  (const QByteArray/ QString &data)
 *   QByteArray base64UrlDecode  (const QString &b64)
 *   QString    toHex            (const QByteArray &data)
 *   QByteArray fromHex          (const QString &hex)
 *   QByteArray xorCipher        (const QByteArray &data, const QByteArray &key)
 */

#ifndef QTUTIL_CRYPTO_H
#define QTUTIL_CRYPTO_H

#include <QString>
#include <QByteArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

namespace QtUtil {

// ---------------------------------------------------------------------------
// hashHex — generic hash of raw bytes as a lower-case hex string.
// ---------------------------------------------------------------------------
inline QString hashHex(const QByteArray &data, QCryptographicHash::Algorithm algo)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, algo).toHex());
}

// Convenience digests. Each has a QByteArray and a QString (UTF-8) overload.
inline QString md5(const QByteArray &data)    { return hashHex(data, QCryptographicHash::Md5); }
inline QString md5(const QString &data)        { return md5(data.toUtf8()); }
inline QString sha1(const QByteArray &data)   { return hashHex(data, QCryptographicHash::Sha1); }
inline QString sha1(const QString &data)       { return sha1(data.toUtf8()); }
inline QString sha256(const QByteArray &data) { return hashHex(data, QCryptographicHash::Sha256); }
inline QString sha256(const QString &data)     { return sha256(data.toUtf8()); }
inline QString sha512(const QByteArray &data) { return hashHex(data, QCryptographicHash::Sha512); }
inline QString sha512(const QString &data)     { return sha512(data.toUtf8()); }

// ---------------------------------------------------------------------------
// hmacHex / hmacSha256 — keyed-hash message authentication code, hex output.
// ---------------------------------------------------------------------------
inline QString hmacHex(const QByteArray &data, const QByteArray &key,
                       QCryptographicHash::Algorithm algo)
{
    return QString::fromLatin1(
        QMessageAuthenticationCode::hash(data, key, algo).toHex());
}

inline QString hmacSha256(const QByteArray &data, const QByteArray &key)
{
    return hmacHex(data, key, QCryptographicHash::Sha256);
}
inline QString hmacSha256(const QString &data, const QString &key)
{
    return hmacSha256(data.toUtf8(), key.toUtf8());
}

// ---------------------------------------------------------------------------
// Base64 (standard, with padding)
// ---------------------------------------------------------------------------
inline QString base64Encode(const QByteArray &data)
{
    return QString::fromLatin1(data.toBase64());
}
inline QString base64Encode(const QString &data) { return base64Encode(data.toUtf8()); }

inline QByteArray base64Decode(const QString &b64)
{
    return QByteArray::fromBase64(b64.toLatin1());
}

// Decode base64 and interpret the result as UTF-8 text.
inline QString base64DecodeText(const QString &b64)
{
    return QString::fromUtf8(base64Decode(b64));
}

// ---------------------------------------------------------------------------
// Base64 (URL-safe, no padding) — suitable for tokens and query parameters.
// ---------------------------------------------------------------------------
inline QString base64UrlEncode(const QByteArray &data)
{
    return QString::fromLatin1(
        data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
inline QString base64UrlEncode(const QString &data) { return base64UrlEncode(data.toUtf8()); }

inline QByteArray base64UrlDecode(const QString &b64)
{
    return QByteArray::fromBase64(
        b64.toLatin1(),
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

// ---------------------------------------------------------------------------
// Hex encoding
// ---------------------------------------------------------------------------
inline QString toHex(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex());
}
inline QByteArray fromHex(const QString &hex)
{
    return QByteArray::fromHex(hex.toLatin1());
}

// ---------------------------------------------------------------------------
// xorCipher — symmetric XOR with a repeating key. Applying it twice with the
// same key restores the original data. Useful for light obfuscation only; it
// is NOT secure encryption. Returns empty if the key is empty.
// ---------------------------------------------------------------------------
inline QByteArray xorCipher(const QByteArray &data, const QByteArray &key)
{
    if (key.isEmpty())
        return QByteArray();
    QByteArray out(data.size(), Qt::Uninitialized);
    for (int i = 0; i < data.size(); ++i)
        out[i] = static_cast<char>(data.at(i) ^ key.at(i % key.size()));
    return out;
}

} // namespace QtUtil

#endif // QTUTIL_CRYPTO_H
